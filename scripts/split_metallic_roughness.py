import cv2
import os
from argparse import ArgumentParser

if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--input", type=str)
    args = parser.parse_args()
    fname, _ = os.path.splitext(args.input)
    img = cv2.imread(args.input)
    cv2.imwrite(fname+"_Metallic.png", img[:,:,0])
    cv2.imwrite(fname+"_Roughness.png", img[:,:,1])
