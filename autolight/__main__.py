"""Entry point for `python -m autolight`."""
import sys

if len(sys.argv) > 1 and sys.argv[1] == "setup":
    from .setup import setup
    import asyncio
    dims = sys.argv[2:] if len(sys.argv) > 2 else None
    asyncio.run(setup(dimensions=dims))
else:
    from .run import main
    import asyncio
    asyncio.run(main())
