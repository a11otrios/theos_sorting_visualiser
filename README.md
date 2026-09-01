# Sorting Visualizer

An interactive, sound-enabled visualization of sorting algorithms, provided in
two implementations:

- `sorting4.py` — cross-platform Python application using Pygame and NumPy.
- `sorting4.cpp` — native Windows application using the Win32 API.

The visualizer includes bubble, selection, quick, heap, pancake, merge,
cocktail shaker, radix, insertion, Tim, radix-merge, and radix-heap sorts. The
native version also includes silly sort. Controls let you change the array size
and animation speed, shuffle the values, and stop a running sort.

## Python version

Python 3.10 or newer is recommended.

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python sorting4.py
```

On macOS or Linux, activate the environment with
`source .venv/bin/activate` instead.

## Native Windows version

The C++ implementation requires Windows, CMake 3.20 or newer, and a C++17
compiler such as Visual Studio 2022.

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\sorting_visualizer.exe
```

For single-configuration generators, the executable may instead be created at
`build\sorting_visualizer.exe`.

## Controls

- Choose an algorithm using the buttons on the right.
- Use **Shuffle** to generate a new arrangement.
- Adjust the size and speed controls at the bottom.
- Press **Escape** to stop or exit. The native version also has a **Stop** button.

## Repository contents

Only the two current implementations and their project support files are
tracked. Local virtual environments, IDE settings, binaries, and build output
are ignored.
