import zlib
import struct
import os
import sys
import pygame

def read_next_chunk(file):
    size_data = file.read(4) #4 bytes = 1 .ppm file, assuming size of ppm is unsigned int
    if not size_data:
        return None
    size = struct.unpack('<I', size_data)[0] #first and only element of tuple, LSB first unsigned int
    data = file.read(size)
    return data

def parse_ppm(data):
    lines = data.split(b'\n')
    width, height = map(int, lines[1].split()) #second line is width and height (whitespace separated) in common ppm struct
    max_val = int(lines[2]) #third line is max pixel val
    pixels = b''
    for line in lines[3:]:
        pixels += line + b'\n'  
    actual_pixels = pixels.rstrip() #remove /n
    return actual_pixels, (width, height) #return tuple with raw pixels and tuple of dimensions

def main():
    if not os.path.exists("reference.vzip"):
        return

    pygame.init()

    with open("reference.vzip", "rb") as f:
        chunk = read_next_chunk(f)
        while chunk:
            lines = zlib.decompress(chunk).split(b'\n')
            width, height = map(int, lines[1].split())
            #second line is width and height (whitespace separated) in common ppm struct
            max_val = int(lines[2]) #third line is max pixel val
            pixels = b''
            for line in lines[3:]:
                pixels += line + b'\n'  
            pixels = pixels.rstrip() #remove /n
            size = (width, height) #return tuple with raw pixels and tuple of dimensions
           
            pixels, size = parse_ppm(zlib.decompress(chunk))
            #get raw pixels and its dimensions from byte obj from decompress
            actual_image = pygame.image.frombuffer(pixels, size, 'RGB')
            if not actual_image:
                raise PygameImageError("Failed to craft image from pixels")
            screen = pygame.display.set_mode(size) #
            screen.blit(actual_image, (0, 0)) #place image at top left
            pygame.display.flip() #update display, show image

            #pygame.time.wait(1000)

            chunk = read_next_chunk(f)

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()

    pygame.quit()

if __name__ == "__main__":
    main()
