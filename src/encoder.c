#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

#define max(a,b) ((a < b) ? b : a)
#define min(a,b) ((a < b) ? a : b)
#define absd(a,b) ((a < b) ? b - a : a - b)

typedef char bool;

//a struct that contains information about an active region
struct region {
  bool is_active;
  int l;
  unsigned char* output;
  unsigned char color;
};

//the bitmask to apply to all values. The last bit must be unset, but it is possible to get better compression (and a more restricted color space) by making a smaller mask.
unsigned char MASK = 0xfe;

//encodes one component of one row.
unsigned char* encode_row(struct region* regions, int rowlen, unsigned char* output, unsigned char* row, unsigned char* nextrow) {
  for(int x = 0; x < rowlen; x++) { //first we have to get rid of the low bit of every byte
    row[x] &= MASK;
  }
  for(int x = 0; x < rowlen;) {
    if (regions[x].is_active) { //if this region is active, we need to see if it continues here or not
      int i = x;
      int imax = min(rowlen-1,x+regions[x].l+1);
      int compare_val = regions[x].color;
      for(;i <= imax; i++) {
        if(row[i] != compare_val) {
          break;
        }
      }
      if (i == imax) { //this region continues. This case also happens when x == rowlen - 1, so we don't have to worry about overflowing in future
        if (x < i) { //if x == imax, then we would be deactivating regions[x], which is a no-no. Generally this applies only on the edge
          regions[i].is_active = 0;
          *(regions[i].output) = regions[i].l | 0x80;
        }
        regions[x].l++;
        if (regions[x].l == 127) { //make sure regions don't get too big
          regions[x].is_active = 0;
          *(regions[x].output) = 0xff;
        }
        x = i + 1;
      } else { //this region is now inactive, so we either make a new region or make a zero-length region
        *(regions[x].output) = regions[x].l | 0x80;
        if ((nextrow != 0) && ((nextrow[x] & row[x]) == row[x]) && ((nextrow[x+1] & row[x]) == row[x])) { //case: this region is nonzero length
          *output = (row[x] - (x == 0 ? 0 : row[x-1])) | 0x1;
          regions[x].color = row[x];
          regions[x].output = ++output;
          regions[x].l = 0;
          output++;
        } else { //case: this region is zero length, so we don't bother with it
          regions[x].is_active = 0;
          *output = row[x] - (x == 0 ? 0 : row[x-1]); //even - even -> even, so we don't need to bother setting bits
          output++;
        }
        x++;
      }
    } else { //no active region, so we either make a new region or make a zero-length region
      if ((nextrow != 0) && ((nextrow[x] & row[x]) == row[x]) && ((nextrow[x+1] & row[x]) == row[x])) { //case: this region is nonzero length
        regions[x].is_active = 1;
        *output = (row[x] - (x == 0 ? 0 : row[x-1])) | 0x1;
        regions[x].color = row[x];
        regions[x].output = ++output;
        regions[x].l = 0;
        output++;
      } else { //case: this region is zero length, so we don't bother with it
        *output = row[x] - (x == 0 ? 0 : row[x-1]); //even - even -> even, so we don't need to bother setting bits
        output++;
      }
      x++;
    }
  }
  return output;
}

//extract the red components
void extract_red(unsigned char* row_buf, unsigned char* row_ptr, int row_len) {
  for(int i = 0; i < row_len; i++) {
    row_ptr[i] = row_buf[3*i];
  }
}

//extract the green components
void extract_green(unsigned char* row_buf, unsigned char* row_ptr, int row_len) {
  for(int i = 0; i < row_len; i++) {
    row_ptr[i] = row_buf[3*i + 1];
  }
}

//extract the blue components
void extract_blue(unsigned char* row_buf, unsigned char* row_ptr, int row_len) {
  for(int i = 0; i < row_len; i++) {
    row_ptr[i] = row_buf[3*i + 2];
  }
}

//reads from a file and outputs to a file, calling encode_row repeatedly and feeding the results into zlib
//reads and writes only one row at a time, so it can work on streams and can easily be adapted to situations where that is key
void encode_stream(FILE* in, FILE* out, int rowlen) {
  z_stream ostream;
  ostream.zalloc = Z_NULL;
  ostream.zfree = Z_NULL;
  ostream.opaque = Z_NULL;
  deflateInit(&ostream,Z_DEFAULT_COMPRESSION);
  struct region* red_regions = calloc(rowlen,sizeof(struct region));
  struct region* green_regions = calloc(rowlen,sizeof(struct region));
  struct region* blue_regions = calloc(rowlen,sizeof(struct region));
  unsigned char* output_buffer = calloc(rowlen*128*6,1); //an adversarial example keeps the output buffer locked for an entire 128 rows, so we need quite a lot of memory to guarantee no segfaults.
  unsigned char* output_cursor = output_buffer;
  int output_begin = 0;
  int output_len = 0;
  unsigned char* compressed_buffer = calloc(rowlen*128*6,1);
  unsigned char* this_row = calloc(rowlen,1);
  unsigned char* next_row = calloc(rowlen,1);
  unsigned char* this_row_buf = calloc(rowlen*3,1);
  unsigned char* next_row_buf = calloc(rowlen*3,1);
  //read the first row into next_row to prime the whole process
  fread(next_row_buf,1,3*rowlen,in);
  while(feof(in) == 0) {
    this_row_buf = next_row_buf;
    fread(next_row_buf,1,3*rowlen,in);
    //encode the different components
    extract_red(this_row_buf,this_row,rowlen);
    extract_red(next_row_buf,next_row,rowlen);
    output_cursor = encode_row(red_regions,rowlen,output_cursor,this_row,next_row);
    extract_green(this_row_buf,this_row,rowlen);
    extract_green(next_row_buf,next_row,rowlen);
    output_cursor = encode_row(green_regions,rowlen,output_cursor,this_row,next_row);
    extract_blue(this_row_buf,this_row,rowlen);
    extract_blue(next_row_buf,next_row,rowlen);
    output_cursor = encode_row(blue_regions,rowlen,output_cursor,this_row,next_row);
    //if we can, input some data.
    while(1) {
      if((output_buffer[output_begin + output_len] & 0x1) && (output_buffer[output_begin + output_len + 1] & 0x80)) {
        output_buffer[output_begin + output_len + 1] &= 0x7f;
        output_len += 2;
      } else if ((output_buffer[output_begin + output_len] & 0x1) == 0) {
        output_len ++;
      } else {
        break;
      }
    }
    if(output_len > 0) {
      ostream.next_in = output_buffer + output_begin;
      ostream.avail_in = output_len;
      ostream.next_out = compressed_buffer;
      ostream.avail_out = rowlen*128*6;
      deflate(&ostream,Z_NO_FLUSH);
      if(ostream.avail_out != rowlen*128*6) {
        fwrite(compressed_buffer,1,rowlen*128*6 - ostream.avail_out,out);
      }
      output_begin += output_len;
      output_len = 0;
      //check if we need to do a memcpy before the next input comes in
      if(output_begin > rowlen*127*6) {
        memcpy(output_buffer,output_buffer + output_begin,rowlen*128*6 - output_begin);
        output_cursor = output_cursor - output_begin;
        output_begin = 0;
      }
    }
  }
  //final row
  extract_red(next_row_buf,next_row,rowlen);
  output_cursor = encode_row(red_regions,rowlen,output_cursor,next_row,0);
  extract_green(next_row_buf,next_row,rowlen);
  output_cursor = encode_row(green_regions,rowlen,output_cursor,next_row,0);
  extract_blue(next_row_buf,next_row,rowlen);
  output_cursor = encode_row(blue_regions,rowlen,output_cursor,next_row,0);
  //finalize all regions that didn't get finalized
  for(int i = 0; i < rowlen; i++) {
    if(red_regions[i].is_active) {
      *(red_regions[i].output) = red_regions[i].l;
    }
    if(green_regions[i].is_active) {
      *(green_regions[i].output) = green_regions[i].l;
    }
    if(blue_regions[i].is_active) {
      *(blue_regions[i].output) = blue_regions[i].l;
    }
  }
  //now 100% of our output should be writeable
  output_len = output_cursor - (output_buffer + output_begin);
  ostream.next_in = output_buffer + output_begin;
  ostream.avail_in = output_len;
  ostream.next_out = compressed_buffer;
  ostream.avail_out = rowlen*128*6;
  deflate(&ostream,Z_FINISH);
  deflateEnd(&ostream);
  fwrite(output_buffer + output_begin, 1, rowlen*128*6 - ostream.avail_out,out);
  free(red_regions);
  free(green_regions);
  free(blue_regions);
  free(output_buffer);
  free(compressed_buffer);
  free(this_row);
  free(next_row);
  free(this_row_buf);
  free(next_row_buf);
}

int main(int argc, char** argv) {
  
}
