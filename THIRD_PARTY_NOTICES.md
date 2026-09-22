# Third-party notices

GrapheneDB's default core build does not vendor or bundle third-party source
libraries.

## Runtime/build dependencies

### Platform threads

The core library uses CMake's `Threads::Threads` abstraction and the threading
implementation supplied by the target operating system/toolchain. No separate
threading library source is bundled in GrapheneDB distributions.

### FAISS (optional; not bundled)

GrapheneDB can be compiled with `GRAPHENEDB_USE_FAISS=ON`. In that mode, the
build locates a FAISS installation supplied separately by the user and links
against it. GrapheneDB does not vendor or redistribute FAISS in the default
source or binary package.

FAISS is published under the MIT license and includes its own third-party
notices. Users who enable or redistribute a FAISS-linked build are responsible
for retaining the applicable FAISS and transitive dependency notices from the
FAISS distribution they use.

Upstream project: https://github.com/facebookresearch/faiss

## Development and CI tooling

CMake, Python, compilers, GitHub Actions, benchmark provider SDK/HTTP services,
and other development infrastructure used to build or test GrapheneDB are not
embedded in the GrapheneDB runtime package unless a future release explicitly
states otherwise.

## Distribution rule

If GrapheneDB later vendors, statically embeds, or redistributes a third-party
component, its applicable license and attribution requirements must be added to
this file before the distribution may be released.
