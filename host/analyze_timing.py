import sys, statistics
import math

PERIOD = 0.020                          # Expected control period, 20 ms

# Read log, keep only timing lines (T,cycle,wake_us,ticks,stale)
rows = []
for line in open(sys.argv[1]):
    if line.startswith("T,"):
        _, cycle, wake_us, ticks, stale = line.strip().split(",")
        rows.append((int(cycle), int(wake_us), int(ticks), int(stale)))

# Check that it has at least two rows
if len(rows) < 2:
    sys.exit("Need at least two timing samples.")

intervals = []

# Count extra timer notifications
extra_notifications = 0
for row in rows:
    extra_notifications += max(row[2] - 1, 0)

# Count missing records between consecutive logged cycle numbers and append intervals
missing_log_samples = 0
for i in range(1, len(rows)):
    cycle_step = rows[i][0] - rows[i - 1][0]
    if cycle_step <= 0:
        sys.exit("Cycle numbers must increase; check for a reset or duplicate records.")
    missing_log_samples += cycle_step - 1			# If nothing wrong, cycle_step = 1
    intervals.append((rows[i][1]-rows[i-1][1])/1e6) # Store all time intervals in seconds

# Measure deviation
deviations = []
for i in range(len(intervals)):
    deviations.append(abs(intervals[i] - PERIOD))

# Nearest-rank 95th percentile, in seconds
sorted_deviations = sorted(deviations)
p95 = sorted_deviations[math.ceil(0.95 * len(sorted_deviations)) - 1]

# Print results 
print(f"Minimum deviation: {min(deviations) * 1000:.3f} ms")
print(f"Median deviation:  {statistics.median(deviations) * 1000:.3f} ms")
print(f"P95 deviation:     {p95 * 1000:.3f} ms")
print(f"Maximum deviation: {max(deviations) * 1000:.3f} ms")
print(f"Extra notifications (recorded cycles): {extra_notifications}")
print(f"Missing log samples (between recorded cycles): {missing_log_samples}")

# Gaps span multiple control cycles, so their intervals cannot be treated
# as measurements of individual control periods.
if missing_log_samples:
    print("Note: interval statistics include gaps across missing log samples.")



    
