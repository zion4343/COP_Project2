import zlib
import struct
import os
import sys
import pygame

#read one .ppm, 4 bytes unsigned int
def read_next(file):
    size_data = file.read(4) 
    if not size_data:
        return None
    size = struct.unpack('<I', size_data)[0] #LSB first
    data = file.read(size)
    return data

def main():
    if not os.path.exists("reference.vzip"):
        return

    pygame.init()

    with open("reference.vzip", "rb") as f:
        ppm = read_next(f)
        
        while ppm:
            lines = zlib.decompress(ppm).split(b'\n')
            #second line of .ppm: width, height
            width, height = map(int, lines[1].split()) 
            pixels = b''
            
            #equalize dimensions
            for line in lines[3:]:
                pixels += line + b'\n'
            pixels = pixels.rstrip()
            size = (width, height)
            
            #display in Pygame
            actual_image = pygame.image.frombuffer(pixels, size, 'RGB')
            if not actual_image:
                raise PygameImageError("Failed to craft image from pixels")       
            screen = pygame.display.set_mode(size)
            screen.blit(actual_image, (0, 0))
            pygame.display.flip()
            ppm = read_next(f)

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
    pygame.quit()

if __name__ == "__main__":
    main()
