# Physical Lattice Capacity Planning

For one axial hex layer of radius `R`, the number of addressable cells is:

```text
capacity(R) = 1 + 3R(R + 1)
```

The minimum radius for a target node count `N` is:

```text
R_min = ceil((-3 + sqrt(12N - 3)) / 6)
```

The current server places nodes on layer 0, so plan against one layer even though the binary format can encode non-negative layer offsets.

| Target nodes | Minimum radius | Recommended operational radius |
|---:|---:|---:|
| 10,000 | 58 | 64 |
| 100,000 | 183 | 200 |
| 1,000,000 | 577 | 600 |
| 10,000,000 | 1,826 | 1,900 |

The recommended radius provides headroom for deletes, placement retries, and growth. A larger radius also increases the maximum sparse file offset, so choose the smallest radius that provides suitable headroom and verify filesystem sparse-file support.

Use both flags in production-like deployments:

```bash
--physical-lattice-radius 600 --expected-max-nodes 1000000
```

Startup fails when the configured capacity is smaller than `--expected-max-nodes`. Runtime capacity is available at:

```text
GET /v1/admin/capacity
```

The readiness endpoint returns `503` if capacity is exhausted. Operators should alert before 80% utilization and plan migration or a larger-radius rebuild before 90%.
