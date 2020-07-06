# comiccompressor
a compression algorithm designed to be efficient for simple images with many solid colors and low entropy (like comics)

The compression strategy is pretty much the same as PNG, with some key differences:

- red, green, and blue segments are processed independently
- the last bit of each byte is stripped out and used for storing extra data. If that bit is set to 1, the next byte lists the size of a triangular region which has the same color. The resulting image is practically indistinguishable from the original in most situations.

The program in this repository does only slightly worse than pngcrush on average, and performs significantly better on some inputs. Inputs and outputs are raw RGB streams.
To build, first make a static build of zlib, and then copy zlib.a into /lib and type 'make'.
