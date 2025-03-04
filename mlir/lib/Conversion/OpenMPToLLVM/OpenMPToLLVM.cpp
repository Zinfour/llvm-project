//===- OpenMPToLLVM.cpp - conversion from OpenMP to LLVM dialect ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/OpenMPToLLVM/ConvertOpenMPToLLVM.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVMPass.h"
#include "mlir/Conversion/LLVMCommon/ConversionTarget.h"
#include "mlir/Conversion/LLVMCommon/Pattern.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Pass/Pass.h"

namespace mlir {
#define GEN_PASS_DEF_CONVERTOPENMPTOLLVMPASS
#include "mlir/Conversion/Passes.h.inc"
} // namespace mlir

using namespace mlir;

namespace {
/// A pattern that converts the region arguments in a single-region OpenMP
/// operation to the LLVM dialect. The body of the region is not modified and is
/// expected to either be processed by the conversion infrastructure or already
/// contain ops compatible with LLVM dialect types.
template <typename OpType>
struct RegionOpConversion : public ConvertOpToLLVMPattern<OpType> {
  using ConvertOpToLLVMPattern<OpType>::ConvertOpToLLVMPattern;

  LogicalResult
  matchAndRewrite(OpType curOp, typename OpType::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto newOp = rewriter.create<OpType>(
        curOp.getLoc(), TypeRange(), adaptor.getOperands(), curOp->getAttrs());
    rewriter.inlineRegionBefore(curOp.getRegion(), newOp.getRegion(),
                                newOp.getRegion().end());
    if (failed(rewriter.convertRegionTypes(&newOp.getRegion(),
                                           *this->getTypeConverter())))
      return failure();

    rewriter.eraseOp(curOp);
    return success();
  }
};

template <typename T>
struct RegionLessOpWithVarOperandsConversion
    : public ConvertOpToLLVMPattern<T> {
  using ConvertOpToLLVMPattern<T>::ConvertOpToLLVMPattern;
  LogicalResult
  matchAndRewrite(T curOp, typename T::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    TypeConverter *converter = ConvertToLLVMPattern::getTypeConverter();
    SmallVector<Type> resTypes;
    if (failed(converter->convertTypes(curOp->getResultTypes(), resTypes)))
      return failure();
    SmallVector<Value> convertedOperands;
    assert(curOp.getNumVariableOperands() ==
               curOp.getOperation()->getNumOperands() &&
           "unexpected non-variable operands");
    for (unsigned idx = 0; idx < curOp.getNumVariableOperands(); ++idx) {
      Value originalVariableOperand = curOp.getVariableOperand(idx);
      if (!originalVariableOperand)
        return failure();
      if (originalVariableOperand.getType().isa<MemRefType>()) {
        // TODO: Support memref type in variable operands
        return rewriter.notifyMatchFailure(curOp,
                                           "memref is not supported yet");
      }
      convertedOperands.emplace_back(adaptor.getOperands()[idx]);
    }
    rewriter.replaceOpWithNewOp<T>(curOp, resTypes, convertedOperands,
                                   curOp->getAttrs());
    return success();
  }
};

struct ReductionOpConversion : public ConvertOpToLLVMPattern<omp::ReductionOp> {
  using ConvertOpToLLVMPattern<omp::ReductionOp>::ConvertOpToLLVMPattern;
  LogicalResult
  matchAndRewrite(omp::ReductionOp curOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    if (curOp.getAccumulator().getType().isa<MemRefType>()) {
      // TODO: Support memref type in variable operands
      return rewriter.notifyMatchFailure(curOp, "memref is not supported yet");
    }
    rewriter.replaceOpWithNewOp<omp::ReductionOp>(
        curOp, TypeRange(), adaptor.getOperands(), curOp->getAttrs());
    return success();
  }
};

template <typename Op>
struct LegalizeDataOpForLLVMTranslation : public ConvertOpToLLVMPattern<Op> {
  using ConvertOpToLLVMPattern<Op>::ConvertOpToLLVMPattern;
  LogicalResult
  matchAndRewrite(Op curOp, typename Op::Adaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    rewriter.replaceOpWithNewOp<Op>(curOp, TypeRange(), adaptor.getOperands(),
                                    curOp.getOperation()->getAttrs());
    return success();
  }
};
} // namespace

void mlir::configureOpenMPToLLVMConversionLegality(
    ConversionTarget &target, LLVMTypeConverter &typeConverter) {
  target.addDynamicallyLegalOp<mlir::omp::CriticalOp, mlir::omp::ParallelOp,
                               mlir::omp::WsLoopOp, mlir::omp::SimdLoopOp,
                               mlir::omp::MasterOp, mlir::omp::SectionsOp,
                               mlir::omp::SingleOp>([&](Operation *op) {
    return typeConverter.isLegal(&op->getRegion(0)) &&
           typeConverter.isLegal(op->getOperandTypes()) &&
           typeConverter.isLegal(op->getResultTypes());
  });
  target.addDynamicallyLegalOp<mlir::omp::AtomicReadOp,
                               mlir::omp::AtomicWriteOp, mlir::omp::FlushOp,
                               mlir::omp::ThreadprivateOp, mlir::omp::DataOp,
                               mlir::omp::EnterDataOp, mlir::omp::ExitDataOp>(
      [&](Operation *op) {
        return typeConverter.isLegal(op->getOperandTypes()) &&
               typeConverter.isLegal(op->getResultTypes());
      });
  target.addDynamicallyLegalOp<mlir::omp::ReductionOp>([&](Operation *op) {
    return typeConverter.isLegal(op->getOperandTypes());
  });
}

void mlir::populateOpenMPToLLVMConversionPatterns(LLVMTypeConverter &converter,
                                                  RewritePatternSet &patterns) {
  patterns.add<
      ReductionOpConversion, RegionOpConversion<omp::CriticalOp>,
      RegionOpConversion<omp::MasterOp>, ReductionOpConversion,
      RegionOpConversion<omp::MasterOp>, RegionOpConversion<omp::ParallelOp>,
      RegionOpConversion<omp::WsLoopOp>, RegionOpConversion<omp::SectionsOp>,
      RegionOpConversion<omp::SimdLoopOp>, RegionOpConversion<omp::SingleOp>,
      RegionLessOpWithVarOperandsConversion<omp::AtomicReadOp>,
      RegionLessOpWithVarOperandsConversion<omp::AtomicWriteOp>,
      RegionLessOpWithVarOperandsConversion<omp::FlushOp>,
      RegionLessOpWithVarOperandsConversion<omp::ThreadprivateOp>,
      LegalizeDataOpForLLVMTranslation<omp::DataOp>,
      LegalizeDataOpForLLVMTranslation<omp::EnterDataOp>,
      LegalizeDataOpForLLVMTranslation<omp::ExitDataOp>>(converter);
}

namespace {
struct ConvertOpenMPToLLVMPass
    : public impl::ConvertOpenMPToLLVMPassBase<ConvertOpenMPToLLVMPass> {
  using Base::Base;

  void runOnOperation() override;
};
} // namespace

void ConvertOpenMPToLLVMPass::runOnOperation() {
  auto module = getOperation();

  // Convert to OpenMP operations with LLVM IR dialect
  RewritePatternSet patterns(&getContext());
  LLVMTypeConverter converter(&getContext());
  arith::populateArithToLLVMConversionPatterns(converter, patterns);
  cf::populateControlFlowToLLVMConversionPatterns(converter, patterns);
  populateFinalizeMemRefToLLVMConversionPatterns(converter, patterns);
  populateFuncToLLVMConversionPatterns(converter, patterns);
  populateOpenMPToLLVMConversionPatterns(converter, patterns);

  LLVMConversionTarget target(getContext());
  target.addLegalOp<omp::TerminatorOp, omp::TaskyieldOp, omp::FlushOp,
                    omp::BarrierOp, omp::TaskwaitOp>();
  configureOpenMPToLLVMConversionLegality(target, converter);
  if (failed(applyPartialConversion(module, target, std::move(patterns))))
    signalPassFailure();
}
