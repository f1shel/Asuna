import cv2
import os
from argparse import ArgumentParser

if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--input", type=str)
    args = parser.parse_args()
    fname, _ = os.path.splitext(args.input)
    img = cv2.imread(args.input)
    img[:,:,1] = 1 - img[:,:,1]
    cv2.imwrite(fname+"_revert.png", img)