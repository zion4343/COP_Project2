import zlib
import struct
import os
import sys
import pygame

def read_next_chunk(file):
    size_data = file.read(4)
    if not size_data:
        return None
    size = struct.unpack('<I', size_data)[0]
    data = file.read(size)
    return data

def parse_ppm(data):
    lines = data.split(b'\n')
    width, height = map(int, lines[1].split())
    max_val = int(lines[2])
    pixels = b''
    for line in lines[3:]:
        pixels += line + b'\n'  
    image_data = pixels.rstrip()  
    return image_data, (width, height)


def main():
    if not os.path.exists("reference.vzip"):
        print("Error reference.vzip")
        return

    pygame.init()

    with open("reference.vzip", "rb") as f:
        chunk = read_next_chunk(f)
        while chunk:
            decompressed_data = zlib.decompress(chunk)

            image_data, image_size = parse_ppm(decompressed_data)

            img = pygame.image.frombuffer(image_data, image_size, 'RGB') 
            if not img:
                print("Error pygame img")
                break

            screen = pygame.display.set_mode(image_size)
            screen.blit(img, (0, 0))
            pygame.display.flip()

            #pygame.time.wait(1000) 

            chunk = read_next_chunk(f)

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()

    pygame.quit()

if __name__ == "__main__":
    main()