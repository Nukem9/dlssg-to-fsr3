# tiny-process-library
A small platform independent library making it simple to create and stop new processes in C++, as well as writing to stdin and reading from stdout and stderr of a new process.

This library was created for, and is used by the C++ IDE project [juCi++](https://gitlab.com/cppit/jucipp).

### Features
* No external dependencies
* Simple to use
* Platform independent
  * Creating processes using executables is supported on all platforms
  * Creating processes using functions is only possible on Unix-like systems
* Read separately from stdout and stderr using anonymous functions
* Write to stdin
* Kill a running process (SIGTERM is supported on Unix-like systems)
* Correctly closes file descriptors/handles

### Usage
See [examples.cpp](examples.cpp).

### Get, compile and run

#### Unix-like systems
```sh
git clone http://gitlab.com/eidheim/tiny-process-library
cd tiny-process-library
mkdir build
cd build
cmake ..
make
./examples
```

#### Windows with MSYS2 (https://msys2.github.io/)
```sh
git clone http://gitlab.com/eidheim/tiny-process-library
cd tiny-process-library
mkdir build
cd build
cmake -G"MSYS Makefiles" ..
make
./examples
```

### Coding style
Due to poor lambda support in clang-format, a custom clang-format is used with the following patch applied:
```diff
diff --git a/lib/Format/ContinuationIndenter.cpp b/lib/Format/ContinuationIndenter.cpp
index bb8efd61a3..e80a487055 100644
--- a/lib/Format/ContinuationIndenter.cpp
+++ b/lib/Format/ContinuationIndenter.cpp
@@ -276,6 +276,8 @@ LineState ContinuationIndenter::getInitialState(unsigned FirstIndent,
 }
 
 bool ContinuationIndenter::canBreak(const LineState &State) {
+  if(Style.ColumnLimit==0)
+    return true;
   const FormatToken &Current = *State.NextToken;
   const FormatToken &Previous = *Current.Previous;
   assert(&Previous == Current.Previous);
@@ -325,6 +327,8 @@ bool ContinuationIndenter::canBreak(const LineState &State) {
 }
 
 bool ContinuationIndenter::mustBreak(const LineState &State) {
+  if(Style.ColumnLimit==0)
+    return false;
   const FormatToken &Current = *State.NextToken;
   const FormatToken &Previous = *Current.Previous;
   if (Current.MustBreakBefore || Current.is(TT_InlineASMColon))
```
