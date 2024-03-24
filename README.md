# Vzip Visualizer

This visualizer is designed to play videos of .vzip format. It decompresses the .ppm frames and display them one by one once in lexicographical order.

## Requirements
- Python (version >= 3.6)
- zlib library
- Pygame (for display)

## Installation
1. Clone this repository to your local machine.
2. Ensure you have Python language installed. You can download it from https://www.python.org/.
3. Install the required dependencies in terminal: pip install pygame.
4. Ensure you have zlib installed. You can download it from https://zlib.net/.

## Build and Run
1. Place [reference.vzip] file and `visualizer.py` in the same directory.
2. Open the terminal in said directory and run the visualizer script: python3 visualizer.py.
3. The video will start displaying frame after frame in lexicographical order. After the last frame, the visualizer will exit.

## File Structure
- `visualizer.py`: The main script that loads and plays the vzip video.
- `README.md`: This file, providing instructions for building and running the code.

## Notes
- This simple visualizer implementation does not include controls for playing, stopping, rewinding and other control functions.
- You can edit the visualizer.py source code to play any .vzip file beside [reference.vzip] by replacing "reference" with your vzip file name.
