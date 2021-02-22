SYMBOL EXPORTS FOR C MACROS

Loading python module
https://stackoverflow.com/questions/67631/how-to-import-a-module-given-the-full-path

import importlib.util
spec = importlib.util.spec_from_file_location("module.name", "/path/to/file.py")
foo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(foo)

