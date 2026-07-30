# Developer onboarding changes

The current developer-preview branch now provides:

- a five-minute path at the top of `README.md`;
- one-command Unix and Windows reasoning demos;
- a clean build/install verifier;
- a separate CMake consumer using the exported `GrapheneDB::graphenedb` target;
- an Ubuntu and Windows developer-experience workflow;
- Markdown and JSON validation receipts.

The verified Unix package path is:

```text
configure
→ build all installable targets
→ install to isolated prefix
→ find_package(GrapheneDB CONFIG REQUIRED)
→ link GrapheneDB::graphenedb
→ run external consumer
```

The first verification attempt identified that building only the library was insufficient because the install manifest also includes the CLI. The committed verifier builds all configured installable targets before installation.
