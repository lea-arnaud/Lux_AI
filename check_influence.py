import json
import numpy as np
import matplotlib.pyplot as plt

# Load data from a JSON file
with open('influence.json', 'r') as json_file:
    data = json.load(json_file)

width = data["w"]
height = data["h"]
heatmap_data = data["map"]

# Convert the 1D map list into a 2D numpy array
heatmap = np.zeros((height, width))

for i, v in enumerate(heatmap_data):
    heatmap[int(i / width), int(i % width)] = heatmap_data[i]

# Flip the array vertically to place (0, 0) at the top left
#heatmap_data = np.flipud(heatmap_data)

# Create a heatmap
plt.imshow(heatmap, cmap='YlOrRd', interpolation='nearest')
plt.colorbar()

# You can customize the plot further if needed, e.g., add labels, titles, etc.

# Show the heatmap
plt.show()