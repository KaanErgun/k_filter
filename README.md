# k_filter - Embedded Filtering Library with Python Simulation

`k_filter` is a lightweight, dependency-free filter library designed specifically for embedded systems. It provides implementations of the most common and useful signal processing filters to smooth out noisy sensor data.

This project also includes a Python-based simulation suite to visualize how each filter works on synthetic noisy data.

---

## ✨ Supported Filters

| Filter Type                 | Description                                                                 |
|----------------------------|-----------------------------------------------------------------------------|
| Moving Average             | Averages the last N samples. Simple and effective for smoothing slow signals. |
| Low-Pass IIR Filter        | Exponentially weighted moving average for real-time applications.           |
| Median Filter              | Good at rejecting outliers and spikes in data.                             |
| Exponential Moving Average | Memory-efficient version of moving average.                                |
| Kalman Filter (1D)         | Estimates true state from noisy measurements with probabilistic reasoning.  |

---

## 📦 Project Structure

```
k_filter/
├── include/                # Header file with filter API
│   └── k_filter.h
├── src/                    # C source file
│   └── k_filter.c
├── examples/               # Usage example in C
│   └── filter_test.c
├── sim/                    # Python simulation
│   ├── filters.py
│   └── simulate.py
├── .gitignore              # Ignores venv, pycache, build outputs
└── README.md               # This file
```

---

## 🔧 Integration Guide

### 1. Copy Files

Copy these files into your embedded project:

```
include/k_filter.h
src/k_filter.c
```

### 2. Update Include Paths (if using Makefile)

```make
CFLAGS += -Ik_filter/include
SRCS += k_filter/src/k_filter.c
```

### 3. Use in Your Code

```c
#include "k_filter.h"

float buffer[5];
MovingAverageFilter ma;

ma_init(&ma, buffer, 5);
float filtered = ma_update(&ma, raw_sensor_value);
```

> ✅ No heap allocation used. No dependencies on standard libraries (except `stdlib.h` for `qsort`).

---

## 🧪 Python Simulation

We’ve also included a Python script to visualize how each filter processes a noisy signal.

### ✅ Requirements

Create a virtual environment and install dependencies:

```bash
python3 -m venv venv
source venv/bin/activate
pip install numpy matplotlib
```

### ▶️ Run the Simulation

```bash
python sim/simulate.py
```

It will generate and show a 6-panel graph:

1. Clean vs noisy input  
2. Output of each filter

This helps visually understand the difference between filters.

---

## 📈 Filter Use Cases

| Application          | Filter Suggestions                                     |
|----------------------|--------------------------------------------------------|
| Temperature sensor   | Moving Average or EMA                                  |
| Robot arm position   | Kalman or Low-Pass                                     |
| ECG or heart rate    | Median or Kalman                                       |
| Accelerometer data   | EMA or Low-Pass                                        |
| Real-time dashboards | EMA or Kalman                                          |

---

## 🛑 .gitignore Highlights

We exclude the following:

```
venv/
__pycache__/
simulated_filters.png
*.o
*.elf
.DS_Store
```

---

## 🤝 Contributing

Pull requests are welcome! Add filters, optimize memory usage, or extend the Python simulation.

---

## 📜 License

MIT License — free for personal and commercial use.
