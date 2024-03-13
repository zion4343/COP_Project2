import zlib

def main(filename):
    # is it better to make this in other function
    with open(filename, 'r') as f:
        data = f.read()
        decompressed_data = zlib.decompress(data)
        num_frames = len(decompressed_data)
        #for i in range (num_frames):
            #trying to decompress the frames stored in vzip file and converting them into surface
            #not sure if this idea would work
