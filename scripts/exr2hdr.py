import cv2
import os
from argparse import ArgumentParser

def exr2hdr(file_path):
    exr = cv2.imread(file_path, cv2.IMREAD_UNCHANGED|cv2.IMREAD_ANYDEPTH)
    assert exr is not None, "failed to open input image!"
    assert exr.shape[2] >= 3, "image should at least contain 3 channels!"
    path, filename = os.path.split(file_path)
    name, _ = os.path.splitext(filename)
    if exr.shape[2] != 3:
        print("[!] abort some channels, only preserve channel [0:3]")
    cv2.imwrite(os.path.join(path, name + ".hdr"), exr[:,:,0:3])

if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--exr", type=str, default="")
    parser.add_argument("--dir", type=str, default=".")
    args = parser.parse_args()
    
    if args.exr != "":
        exr2hdr(args.exr)
    else:
        files = os.listdir(args.dir)
        for file in files:
            if file.endswith(".exr"):
                print("[ ] processing %s..." % file)
                exr2hdr(os.path.join(args.dir, file))
                print("[ ] done!\n")