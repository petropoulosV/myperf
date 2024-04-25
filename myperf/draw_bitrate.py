#!/usr/bin/python

import matplotlib.pyplot as plt;
import sys;

x = []
for t in range(0, 21):
	x.append(t)

# Dataset
# --------

y1 = [0, 101.68, 165.59, 8.66, 8.86, 8.58, 8.62, 129.51, 206.65, 208.52, 203.64, 202.95, 207.21, 204.96, 10.15, 9.65, 10.20, 9.85, 8.36, 8.13, 50.06]

y2 = [0, 316.05, 260.79, 265.83, 71.31, 259.21, 250.65, 231.54, 17.11, 229.69, 257.87, 234.08, 316.36, 240.54, 233.47, 41.26, 191.48, 19.80, 19.81, 46.78, 75.21]

y3 = [0, 491.92, 497.13, 498.15, 494.68, 491.24, 494.97, 496.87, 493.74, 496.49, 493.51, 491.96, 500.87, 497.34, 489.83, 500.70, 491.94, 494.26, 495.28, 497.95, 577.79]

y4 = [0, 506.01, 498.45, 501.14, 497.02, 496.45, 499.58, 497.15, 499.10, 493.41, 498.12, 496.18, 502.20, 491.52, 500.70, 503.11, 500.00, 499.15, 501.63, 502.94, 575.49]

y5 = [0, 46.95, 7.03, 148.86, 161.77, 162.41, 162.54, 115.28, 6.82, 7.54, 7.91, 7.67, 7.82, 7.30, 116.11, 162.58, 166.30, 163.55, 164.13, 164.20, 164.01]

y6 = [0, 110.95, 17.68, 13.72, 254.54, 36.38, 84.40, 132.21, 230.70, 124.15, 16.94, 33.24, 48.34, 106.82, 67.84, 254.76, 210.95, 209.39, 257.47, 259.80, 259.27]

y7 = [0, 575.86, 499.83, 500.55, 497.58, 504.55, 503.03, 498.15, 505.34, 495.74, 504.04, 502.05, 499.32, 500.57, 504.26, 498.51, 500.18, 502.25, 501.72, 498.76, 499.33]

y8 = [0, 568.51, 504.51, 496.87, 500.83, 498.17, 501.34, 502.86, 497.83, 500.01, 498.04, 505.25, 500.27, 499.46, 495.45, 496.54, 493.92, 501.51, 495.02, 496.42, 497.81]

# Draw Plot
# ---------

plt.figure(figsize=(8, 5))

# plt.title("Node A & C to B, 100 Kbits/sec")
# plt.title("Node A & C to B, 1 Mbit/sec")
# plt.title("Node A & C to B, 10 Mbits/sec")
# plt.title("Node A & C to B, 100 Mbits/sec")
plt.title("Node A & C to B, 1000 Mbits/sec")

plt.xlabel("Time (s)")

# plt.ylabel("Throughput (Kbits/sec)")
plt.ylabel("Throughput (Mbits/sec)")

# plt.ylim(top=200)
# plt.ylim(top=2)
# plt.ylim(top=20)
# plt.ylim(top=200)

# plt.plot(x, y1, lw = 0.75, label = "Node A w/ 16-byte window")
# plt.plot(x, y2, lw = 0.75, label = "Node A w/ 64-byte window")
# plt.plot(x, y3, lw = 0.75, label = "Node A w/ 512-byte window")
# plt.plot(x, y4, lw = 0.75, label = "Node A w/ 1024-byte window")

plt.plot(x, y5, lw = 0.75, label = "Node C w/ 16-byte window")
plt.plot(x, y6, lw = 0.75, label = "Node C w/ 64-byte window")
plt.plot(x, y7, lw = 0.75, label = "Node C w/ 512-byte window")
plt.plot(x, y8, lw = 0.75, label = "Node C w/ 1024-byte window")

plt.margins(0.02)
plt.legend()
plt.grid()

plt.show()
# plt.savefig("results/svg/Q7 1G [C].svg")
