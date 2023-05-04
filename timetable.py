# Importing the matplotlib.pyplot
import matplotlib.pyplot as plt
import csv
import matplotlib.colors as mcolors
import matplotlib.patches as mpatches
import itertools

import sys

if len(sys.argv) < 2:
    print("Please give filename")
    exit(64)

filename = sys.argv[1]

datapoints = []
names = set()
count = 0

with open(filename, 'r') as csvfile:
    datareader = csv.reader(csvfile)
    for row in datareader:
        count += 1
        if len(row) > 0 and row[0] == 'taskdebug':
            datapoints.append((int(row[1]), int(row[2]), int(row[3]), row[5]))
            names.add(row[5])

datapoints.sort(key=lambda x: x[0])
names = list(names)
names.sort()
groups = [(i, list(group)) for (i, group) in itertools.groupby(datapoints, lambda x: x[0])]

# print(datapoints)
# Declaring a figure "gnt"
fig, gnt = plt.subplots()
 
# Setting Y-axis limits
# gnt.set_ylim(0, 50)
 
# # Setting X-axis limits
# gnt.set_xlim(0, 160)
 
# Setting labels for x-axis and y-axis
gnt.set_xlabel('microseconds since start')
gnt.set_ylabel('Gtid')

# Setting ticks on y-axis
gnt.set_yticks(list(map(lambda x: x+0.5, range(len(groups)))))
# # Labelling tickes of y-axis
gnt.set_yticklabels(list(map(lambda x: str(x[0]), groups)))
 
# Setting graph attribute
# gnt.grid(True)
colors = list(mcolors.TABLEAU_COLORS.values())
mapping = {}
for x, y in zip(colors, names):
    mapping[y] = x


for i, (gtid, group) in enumerate(groups):
    gnt.broken_barh(list(map(lambda x: (x[1], x[2] - x[1]), group)), (i, 0.9), facecolors=list(map(lambda x: mapping[x[3]], group)), alpha=0.5, edgecolor='black', linewidth=1)

legend_data = []
for x, y in mapping.items():
    legend_data.append(mpatches.Patch(color=y, label=x))
gnt.legend(handles=legend_data)

plt.show()