# std/stats

```nori
import "std/stats" as stats
```

std/stats: summary statistics over a `Vec<Float>`. Every function is total: an empty input
yields 0.0 (median/percentile too). Inputs are never mutated (median/percentile sort a copy).
  import "std/stats" as stats
  stats::mean(xs)            // arithmetic mean
  stats::stddev(xs)          // population standard deviation
  stats::percentile(xs, 90.0)
### `fn sum(v: Vec<Float>) -> Float`

sum of all elements.

### `fn mean(v: Vec<Float>) -> Float`

arithmetic mean (0.0 for an empty input).

### `fn min(v: Vec<Float>) -> Float`

smallest element (0.0 if empty).

### `fn max(v: Vec<Float>) -> Float`

largest element (0.0 if empty).

### `fn variance(v: Vec<Float>) -> Float`

population variance (mean of squared deviations).

### `fn stddev(v: Vec<Float>) -> Float`

population standard deviation (sqrt of the variance).

### `fn median(v: Vec<Float>) -> Float`

median (50th percentile): average of the two middle elements for an even count.

### `fn percentile(v: Vec<Float>, p: Float) -> Float`

the `p`-th percentile, `p` in [0,100] (linear interpolation between ranks).


