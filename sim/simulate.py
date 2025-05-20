import numpy as np
import matplotlib.pyplot as plt
from filters import MovingAverage, LowPass, Median, EMA, Kalman1D

# Gürültülü sinyal üret
np.random.seed(42)
x = np.linspace(0, 10, 200)
clean = np.sin(x)
noise = np.random.normal(0, 0.5, x.shape)
noisy = clean + noise

# Filtreleri tanımla
ma = MovingAverage(5)
lp = LowPass(0.1)
med = Median(5)
ema = EMA(0.1)
kalman = Kalman1D(error_measure=0.25, error_estimate=1.0, initial_estimate=0)

# Filtrelenmiş sinyalleri üret
ma_filtered = [ma.update(s) for s in noisy]
lp_filtered = [lp.update(s) for s in noisy]
med_filtered = [med.update(s) for s in noisy]
ema_filtered = [ema.update(s) for s in noisy]
kalman_filtered = [kalman.update(s) for s in noisy]

# Grafik çiz
plt.figure(figsize=(12, 10))

plt.subplot(3, 2, 1)
plt.plot(x, noisy, label="Noisy")
plt.plot(x, clean, label="Clean")
plt.title("Raw vs Clean")
plt.legend()

plt.subplot(3, 2, 2)
plt.plot(x, noisy, label="Noisy")
plt.plot(x, ma_filtered, label="Moving Avg")
plt.title("Moving Average")
plt.legend()

plt.subplot(3, 2, 3)
plt.plot(x, noisy, label="Noisy")
plt.plot(x, lp_filtered, label="Low Pass")
plt.title("Low-Pass Filter")
plt.legend()

plt.subplot(3, 2, 4)
plt.plot(x, noisy, label="Noisy")
plt.plot(x, med_filtered, label="Median")
plt.title("Median Filter")
plt.legend()

plt.subplot(3, 2, 5)
plt.plot(x, noisy, label="Noisy")
plt.plot(x, ema_filtered, label="EMA")
plt.title("Exponential Moving Average")
plt.legend()

plt.subplot(3, 2, 6)
plt.plot(x, noisy, label="Noisy")
plt.plot(x, kalman_filtered, label="Kalman")
plt.title("Kalman Filter")
plt.legend()

plt.tight_layout()
plt.savefig("simulated_filters.png")
plt.show()
