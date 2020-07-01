#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max(a,b) ((a < b) ? b : a)
#define min(a,b) ((a < b) ? a : b)

//a struct that contains information about an active region
struct region {
  char is_active;
  char type;
  unsigned char color;
  unsigned char l;
  unsigned char* output_loc;
  int x0;
  int y0;
};

//default function, identity
int default_f(int i) {
  return i;
}

//encodes one value of one part(red,green,or blue) of the color of a pixel
//
//val = the color value to encode
//left_down = the color value of the pixel at relative index (x,y) = (-1, 1)
//down = the color value of the pixel at relative index (0,1)
//right_down = the color value of the pixel at relative index (1,1)
//right = the color value of the pixel at relative index (1,0)
//x = the current absolute x-coordinate (column)
//y = the current absolute y-coordinate (row)
//region_tracker = a contiguous block of region structs
//regions_max = length of region_tracker.
//regions_top = pointer to an integer indicating the most regions ever used at one time over the course of encoding
//output_loc = pointer to the triple of bytes that will hold the encoded value
//last_type = the last type outputted
//f = the function which controls the difference cutoff based on taxicab distance. Must have f(i+1) >= f(i), f(0) = 0, f(127) <= 127
//
//return value is a pointer to the output position of the region that was overwritten, or 0 if no region was overwritten.
char encode_next(
    unsigned char val,
    unsigned char left_down, unsigned char down, unsigned char right_down, unsigned char right,
    unsigned int x, unsigned int y, 
    struct region* region_tracker, int regions_max, int* regions_top,
    unsigned char* output_loc,
    char last_type,
    int(*f)(int)) {
  //first, go through the current list of regions and deactivate the ones that need deactivating, and build up the weighted average of the ones that apply to this pixel
  printf("encoding value %u at (%u,%u)\n",val,x,y);
  int new_index = -1;
  unsigned long avg_numerator = 0;
  unsigned long avg_denominator = 0;
  for (int i = 0; i < *regions_top; i++) {
    int xoffset = x - region_tracker[i].x0;
    int yoffset = y - region_tracker[i].y0; //guaranteed 0 <= yoffset < 65
    if (region_tracker[i].is_active) {
      //printf("region %u offsets: x = %u, y = %u\n",i,xoffset,yoffset);
      switch(region_tracker[i].type) {
        case 1: {
                  //check for if we are in the region
                  if ((xoffset >= 0) && (xoffset <= yoffset)) {
                    //check for if the region includes this pixel
                    if ((unsigned char)(val - region_tracker[i].color) > f(abs(xoffset) + yoffset)) {
                      region_tracker[i].l = yoffset - 1;
                    //if it does, apply the region
                    } else {
                      printf("applying region %u\n",i);
                      printf("diff = %u, f = %u\n",(unsigned char)(val - region_tracker[i].color),f(abs(xoffset) + yoffset));
                      avg_numerator += ((unsigned int)region_tracker[i].color)*(128 - f(abs(xoffset) + yoffset));
                      avg_denominator += 128 - f(abs(xoffset) + yoffset);
                    }
                  }
                  break;
                }
        case 2: {
                  //check for if we are in the region
                  if ((xoffset >= yoffset) && (xoffset <= region_tracker[i].l)) {
                    //check for if the region includes this pixel
                    if ((unsigned char)(val - region_tracker[i].color) > f(abs(xoffset) + yoffset)) {
                      region_tracker[i].l = abs(xoffset) - 1;
                      //if it does, apply the region
                    } else {
                      printf("applying region %u\n",i);
                      printf("diff = %u, f = %u\n",(unsigned char)(val - region_tracker[i].color),f(abs(xoffset) + yoffset));
                      avg_numerator += ((unsigned int)region_tracker[i].color)*(128 - f(abs(xoffset) + yoffset));
                      avg_denominator += 128 - f(abs(xoffset) + yoffset);
                    }
                  }
                  break;
                }
        case 3: {
                  //check for if we are in the region
                  if ((xoffset >= -1*yoffset) && (xoffset <= 0)) {
                    //check for if the region includes this pixel if ((unsigned char)(val - region_tracker[i].color) > f(abs(xoffset) + yoffset)) { region_tracker[i].l = yoffset - 1;
                    if ((unsigned char)(val - region_tracker[i].color) > f(abs(xoffset) + yoffset)) {
                      region_tracker[i].l = abs(xoffset) - 1;
                    //if it does, apply the region
                    } else {
                      printf("applying region %u\n",i);
                      printf("diff = %u, f = %u\n",(unsigned char)(val - region_tracker[i].color),f(abs(xoffset) + yoffset));
                      avg_numerator += ((unsigned int)region_tracker[i].color)*(128 - f(abs(xoffset) + yoffset));
                      avg_denominator += 128 - f(abs(xoffset) + yoffset);
                    }
                  }
                  break;
                }
        case 4: {
                  //check for if we are in the region
                  if ((xoffset >= 0) && (xoffset <= region_tracker[i].l - yoffset)) {
                    //check for if the region includes this pixel
                    if ((unsigned char)(val - region_tracker[i].color) > f(abs(xoffset) + yoffset)) {
                      region_tracker[i].l = abs(xoffset) - 1 + yoffset;
                    } else {
                      printf("applying region %u\n",i);
                      printf("diff = %u, f = %u\n",(unsigned char)(val - region_tracker[i].color),f(abs(xoffset) + yoffset));
                      avg_numerator += ((unsigned int)region_tracker[i].color)*(128 - f(abs(xoffset) + yoffset));
                      avg_denominator += 128 - f(abs(xoffset) + yoffset);
                    }
                  }
                  break;
                }
        default: break; //nonexistent case
      }
      //if the region has officially expired
      if (region_tracker[i].l < yoffset) {
        //indicate it is completed
        region_tracker[i].output_loc[2] = max(region_tracker[i].l,0) | 0x80;
        printf("region %u expired, type is %u, l is %u, color is %u\n",i,region_tracker[i].type,region_tracker[i].l,region_tracker[i].color);
        if (new_index < 0) {
          new_index = i;
        } else {
          region_tracker[i].is_active = 0;
        }
      }
    } else if (new_index < 0) {
      new_index = i;
    }
  }
  unsigned char c = 0; 
  if(avg_denominator != 0) {
    c = (unsigned char)(avg_numerator/avg_denominator); //unless there are no regions active, set c to be weighted average of all the applied regions.
  }
  //if we didn't find an empty region struct, increment regions_top
  if (new_index < 0) {
    new_index = (*regions_top)++;
  }
  //start filling up the new region struct
  region_tracker[new_index].is_active = 1;
  //try to choose a good type
  unsigned char ld_diff = val - left_down;
  unsigned char d_diff = val - down;
  unsigned char rd_diff = val - right_down;
  unsigned char r_diff = val - right;
  //use a heuristic to choose which order we test different region types in:
  //type 2 fits nicely after type 1
  //types 1 and 4 fit nicely after type 3
  //default to trying the types in order, and if no type works just pick 1
#define test_type1 if ((d_diff <= f(1)) && (rd_diff <= f(2))) { region_tracker[new_index].type = 1; } else
#define test_type2 if ((r_diff <= f(1)) && (rd_diff <= f(2))) { region_tracker[new_index].type = 2; } else
#define test_type3 if ((d_diff <= f(1)) && (ld_diff <= f(2))) { region_tracker[new_index].type = 3; } else
#define test_type4 if ((r_diff <= f(1)) && (d_diff <= f(1))) { region_tracker[new_index].type = 4; } else
#define end_tests { region_tracker[new_index].type = 1; }
  switch (last_type) {
    case 1: {
              test_type2 test_type1 test_type3 test_type4 end_tests
              break;
            }
    case 3: {
              test_type1 test_type4 test_type2 test_type3 end_tests
              break;
            }
    default: {
               test_type1 test_type2 test_type3 test_type4 end_tests
               break;
            }
  }
  //choose the max size of the new region
  //first off, if our value exactly matches the predicted value, output a very small region, since nearby areas are probably predicted well too
  if (c == val) {
    region_tracker[new_index].l = 1;
  } else if (*regions_top >= regions_max/2) { //then start ratcheting down the sizes of regions according to how much of the available space we have used
    region_tracker[new_index].l = 31;
  } else if (*regions_top >= 3*regions_max/4) {
    region_tracker[new_index].l = 15;
  } else if (*regions_top >= 7*regions_max/8) {
    region_tracker[new_index].l = 7;
  } else if (*regions_top >= regions_max - 1) { //we literally only have one region left; leave it deactivated forever and output a 0-length region
    printf("outputting 1-pixel region\n");
    output_loc[0] = 0;
    output_loc[1] = val - c;
    output_loc[2] = 0x80;
    region_tracker[new_index].is_active = 0; //deactivate the region we were using.
    return 0;
  } else {
    region_tracker[new_index].l = 63;
  }
  region_tracker[new_index].x0 = x;
  region_tracker[new_index].y0 = y;
  region_tracker[new_index].output_loc = output_loc;
  region_tracker[new_index].color = val;
  printf("new region %u: type = %u, offset = %u, l = %u\n",new_index,region_tracker[new_index].type,(unsigned char)(val-c),region_tracker[new_index].l);
  //output the encoded value to output_loc
  output_loc[0] = region_tracker[new_index].type;
  output_loc[1] = val - c;
  output_loc[2] = 0;
  return region_tracker[new_index].type;
}

//holds a pointer to an array and its size
struct LZW_dict_entry {
  unsigned short prefix_index;
  unsigned char postfix_value;
  //unsigned short len;
};

//get the index of the dict entry with the given prefix index and postfix
//arguments:
//
//dict = the LZW dictionary
//dict_size = the first unused index in the dict
//last_prefix = the prefix index of the string (-1 for empty)
//this_postfix = the postfix of the string
//
//return value is the dict prefix matching that (prefix,postfix) pair or -1 if no such entry exists
int next_prefix_index(
    struct LZW_dict_entry* dict, unsigned int dict_size,
    int last_prefix, unsigned char this_postfix) {
  if (last_prefix == -1) { //-1 -> 0-length prefix -> new prefix is just the postfix
    return (int)this_postfix;
  }
  for(unsigned int j = 0; j < dict_size; j++) {
    if((dict[j].prefix_index == last_prefix) && (dict[j].postfix_value == this_postfix)) {
      return j + 256;
    }
  }
  return -1;
}

//outputs an LZW-compressed code
//uses a variable-width encoding, width starts at 9, and it is encoded in big endian.
//also adds the new prefix+postfix pair to the dictionary if necessary.
//arguments:
//
//prefix = the prefix index for this string
//postfix = the postfix of this string
//output = pointer to the output buffer
//index = pointer to the current output index
//bitoffset = pointer to the current bit offset in the output
//output_length = pointer to the current length of the output buffer
//current_bit_width = the bit width of the last index to be outputted
//dict = the LZW dict
//dict_nextindex = the next index to use in the dict
//dict_len = maximum length of the dict
void LZW_output(
    int prefix, unsigned char postfix,
    unsigned char** output, unsigned int* index, unsigned int* bitoffset, unsigned int* output_length, unsigned int* current_bit_width,
    struct LZW_dict_entry* dict, unsigned int* dict_nextindex, unsigned int dict_len) {
  //check if we have run out of indexes for the dictionary. If not, store stuff in the dictionary.
  if (*dict_nextindex < dict_len) {
    //first, input the pair into the dictionary
    dict[*dict_nextindex].prefix_index = prefix;
    dict[*dict_nextindex].postfix_value = postfix;
    //if we need to, increase the bit width
    if(1 << (*current_bit_width + 1) == (*dict_nextindex + 256)) {
      *current_bit_width += 1;
    }
    *dict_nextindex += 1;
  }
  //now start outputting
  //expand the output buffer if necessary. We could use up to 2 new bytes.
  if (*index + 2 >= *output_length - 1) {
    *output = realloc(*output,(*output_length)*2);
    *output_length *= 2;
  }
  //see how many bytes we need. The biggest bit-width we support is 16, so we use at most 3 bytes.
  if (*bitoffset + *current_bit_width > 16) { //need three bytes, and bit width is greater than 8
    //first mask is 0xff00, then shift masked prefix by bitoffset + 8 to get just the first 8 - bitoffset bits of prefix
    (*output)[*index] |= (prefix & 0xff00) >> (*bitoffset + 8);
    //second mask is 0xff00 shifted by 8-bitoffset to get the next 8 bits of prefix
    //then shift masked prefix by bitoffset so the first masked bit is at the start of the char.
    (*output)[*index + 1] = (prefix & (0xff00 >> (8 - *bitoffset))) >> *bitoffset;
    //third mask is the last bitwidth - 8 - (8 - bitwidth) bits of prefix, so we shift 0xff by 8 - (current_bit_width + bitoffset - 16) = 24 - current_bit_width - bitoffset
    //then shift the masked value left by 8 - (24 - bitoffset - bit_width) = 16 - bitoffset - bit_width so the first bit of the mask is at the start of the char.
    (*output)[*index + 2] = (prefix & (0xff >> (24 - *bitoffset - *current_bit_width))) << (*bitoffset + *current_bit_width - 16);
    *index += 2;
    *bitoffset = 24 - *bitoffset - *current_bit_width;
  } else { //bit width is >9, so we always use at least 2 bytes.
    //first mask is 0xff00, then shift masked prefix by bitoffset + 8 to get just the first 8 - bitoffset bits of prefix.
    (*output)[*index] |= (prefix & 0xff00) >> (*bitoffset + 8);
    //second mask is the last bitwidth - (8 - bitoffset) <= 8 bits, so we shift 0xff right by 8 - (bitwidth + bitoffset - 8) = 16 - bitwidth - bitoffset
    //then shift the masked value left by 8 - (16 - bitoffset - bit_width) = bitoffset + bit_width - 8 so the first bit of the mask is at the start of the char.
    (*output)[*index + 1] = (prefix & (0xff >> (16 - *bitoffset - *current_bit_width)))  << (*bitoffset + *current_bit_width - 8);
    *index += 1;
    *bitoffset = 16 - *bitoffset - *current_bit_width;
  }
}

//encodes raw image data into a compressed format using LZW and the encode_next function.
//TODO: change this a little so it operates on file streams rather than in-memory buffers
//arguments:
//
//input = input buffer
//columns, rows = columns and rows of the image
//region_tracker_limit = amount of region-tracking structs to allocate for each color segment
//LZW_max_bit_width = maximum bit width to encode LZW stuff in (max supported is 16)
//f = distance function to use
//output_len1 = length of the output buffer that is returned.
//TEST = flag to see if LZW compression should be done (0 for yes)
//
//return value is the output buffer
unsigned char* encode_buffer(
    unsigned char* input, int columns, int rows,
    int region_tracker_limit,
    int LZW_max_bit_width,
    int(*f)(int),
    unsigned int* output_len1,
    int TEST) {
  //we need some state to encode the image
  //memory usage is region_tracker_limit*183 bytes plus a few kilobytes
  //it is unclear how this size affects compression efficiency; definitely a thing to test
  //the adversarial example would be a large blob of a single color, which would be compressed significantly by LZW but involves lots of large regions.
  struct region* region_tracker_red = calloc(region_tracker_limit,sizeof(struct region));
  int region_tracker_red_top = 0;
  char last_type_red;
  struct region* region_tracker_green = calloc(region_tracker_limit,sizeof(struct region));
  int region_tracker_green_top = 0;
  char last_type_green;
  struct region* region_tracker_blue = calloc(region_tracker_limit,sizeof(struct region));
  int region_tracker_blue_top = 0;
  char last_type_blue;
  unsigned char* LZW_input = calloc(region_tracker_limit*18,1); //we use this as a rolling input buffer. Never fills up completely because only region_tracker_limit regions can be active at a time.
  unsigned int LZW_input_begin = 0;
  unsigned int LZW_input_end = 0;
  unsigned int LZW_input_len = 0;
  int LZW_prefix_start = -1;
  struct LZW_dict_entry* LZW_dict = calloc((1 << (LZW_max_bit_width)) - 256,sizeof(struct LZW_dict_entry)); //contains the first 256 bytes as imaginary indexes. 
  unsigned int LZW_dict_nextindex = 0;
  unsigned int dict_max_len = (1 << LZW_max_bit_width) - 256;
  unsigned int LZW_index_bitwidth = 9;
  unsigned char* output = calloc(4096,1);    //the output buffer. Will probably get reallocated.
  unsigned int output_bitoffset = 0;
  unsigned int output_index = 0;
  unsigned int output_len = 4096;
#define get_red(X,Y) (((X < columns) && (Y < rows)) ? input[3*(Y*columns + X)] : 0)
#define get_green(X,Y) (((X < columns) && (Y < rows)) ? input[3*(Y*columns + X) + 1] : 0)
#define get_blue(X,Y) (((X < columns) && (Y < rows)) ? input[3*(Y*columns + X) + 2] : 0)
  //basic algorithm:
  //1) read and encode a value(first reds, then greens, then blues) for each row
  //2) add any additional triples that were free'd up into the LZW input
  //3) if there is LZW output, output it.
  for(int y = 0; y < rows; y++) {
    printf("encoding red values of row %u\n",y);
    for(int x = 0; x < columns; x++) {
      last_type_red = encode_next(
          get_red(x,y),
          get_red(x-1,y+1),get_red(x,y+1),get_red(x+1,y+1),get_red(x+1,y),
          x, y,
          region_tracker_red, region_tracker_limit, &region_tracker_red_top,
          LZW_input+LZW_input_end,
          last_type_red,
          f);
      //triples OR the last bit with 0x80 when they are done. We find those and increment input_len as appropriate.
      //LZW_input_begin and LZW_input_len aren't necessary aligned to 3 bytes, but their sum always is
      while((LZW_input[(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18) + 2] & 0x80) != 0) {
        LZW_input[(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18) + 2] &= 0x7f;
        LZW_input_len += 3;
      }
      //if we're testing, don't do LZW compression, just test to see if the initial encoder works
      if (TEST && (LZW_input_len > 0)) {
        printf("reallocating output buffer from size %u to %u\n",output_len,output_len + LZW_input_len + 6);
        if (output_index + LZW_input_len >= output_len) {
          output = realloc(output,output_len + LZW_input_len + 6);;
          output_len += LZW_input_len + 6;
        }
        printf("writing %u bytes to %p\n",min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin),output + output_index);
        memcpy(output + output_index,LZW_input + LZW_input_begin,min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin));
        if (LZW_input_len > region_tracker_limit*18 - LZW_input_begin) {
          printf("writing %u bytes to %p\n",(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18),output + output_index + region_tracker_limit*18 - LZW_input_begin);
          memcpy(output + output_index + region_tracker_limit*18 - LZW_input_begin,LZW_input,(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18));
        }
        LZW_input_begin = (LZW_input_begin + LZW_input_len) % (region_tracker_limit*18);
        printf("wrote %u bytes\n",LZW_input_len);
        output_index += LZW_input_len;
        LZW_input_len = 0;
      } else {
      //if we have anything to input, input it until we get a string not in the dict
      while(LZW_input_len > 0) {
        int new_prefix = next_prefix_index(
            LZW_dict, LZW_dict_nextindex,
            LZW_prefix_start, LZW_input[LZW_input_begin]);
        //current string is still in the dict, so increment LZW_input_begin and continue
        if (new_prefix >= 0) {
          LZW_input_begin = (LZW_input_begin + 1) % (region_tracker_limit*18);
          LZW_input_len--;
          LZW_prefix_start = new_prefix;
        } else {
          //current prefix + new postfix is not in the dict, so we output the current prefix and to not increment input
          LZW_output(
              LZW_prefix_start,LZW_input[LZW_input_begin],
              &output, &output_index, &output_bitoffset, &output_len, &LZW_index_bitwidth,
              LZW_dict, &LZW_dict_nextindex, dict_max_len);
          LZW_prefix_start = -1; //set prefix to be 0-length
        }
      }
      }
      //increment LZW_input_end to put a new triple in it
      LZW_input_end = (LZW_input_end + 3) % (region_tracker_limit*18);
    } 
    //copy/paste the above code for green and blue segments
    printf("encoding green values of row %u\n",y);
    for(int x = 0; x < columns; x++) {
      last_type_green = encode_next(
          get_green(x,y),
          get_green(x-1,y+1),get_green(x,y+1),get_green(x+1,y+1),get_green(x+1,y),
          x, y,
          region_tracker_green, region_tracker_limit, &region_tracker_green_top,
          LZW_input+LZW_input_end,
          last_type_green,
          f);
      //triples OR the last bit with 0x80 when they are done. We find those and increment input_len as appropriate.
      //LZW_input_begin and LZW_input_len aren't necessary aligned to 3 bytes, but their sum always is
      while((LZW_input[(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18) + 2] & 0x80) != 0) {
        LZW_input[(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18) + 2] &= 0x7f;
        LZW_input_len += 3;
      }
      //if we're testing, don't do LZW compression, just test to see if the initial encoder works
      if (TEST && (LZW_input_len > 0)) {
        printf("reallocating output buffer from size %u to %u\n",output_len,output_len + LZW_input_len + 6);
        if (output_index + LZW_input_len >= output_len) {
          output = realloc(output,output_len + LZW_input_len + 6);;
          output_len += LZW_input_len + 6;
        }
        printf("writing %u bytes to %p\n",min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin),output + output_index);
        memcpy(output + output_index,LZW_input + LZW_input_begin,min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin));
        if (LZW_input_len > region_tracker_limit*18 - LZW_input_begin) {
          printf("writing %u bytes to %p\n",(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18),output + output_index + region_tracker_limit*18 - LZW_input_begin);
          memcpy(output + output_index + region_tracker_limit*18 - LZW_input_begin,LZW_input,(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18));
        }
        LZW_input_begin = (LZW_input_begin + LZW_input_len) % (region_tracker_limit*18);
        printf("wrote %u bytes\n",LZW_input_len);
        output_index += LZW_input_len;
        LZW_input_len = 0;
      } else {
      //if we have anything to input, input it until we get a string not in the dict
      while(LZW_input_len > 0) {
        int new_prefix = next_prefix_index(
            LZW_dict, LZW_dict_nextindex,
            LZW_prefix_start, LZW_input[LZW_input_begin]);
        //current string is still in the dict, so increment LZW_input_begin and continue
        if (new_prefix >= 0) {
          LZW_input_begin = (LZW_input_begin + 1) % (region_tracker_limit*18);
          LZW_input_len--;
          LZW_prefix_start = new_prefix;
        } else {
          //current prefix + new postfix is not in the dict, so we output the current prefix and to not increment input
          LZW_output(
              LZW_prefix_start,LZW_input[LZW_input_begin],
              &output, &output_index, &output_bitoffset, &output_len, &LZW_index_bitwidth,
              LZW_dict, &LZW_dict_nextindex, dict_max_len);
          LZW_prefix_start = -1; //set prefix to be 0-length
        }
      }
      }
      //increment LZW_input_end to put a new triple in it
      LZW_input_end = (LZW_input_end + 3) % (region_tracker_limit*18);
    }
    printf("encoding blue values of row %u\n",y);
    for(int x = 0; x < columns; x++) {
      last_type_blue = encode_next(
          get_blue(x,y),
          get_blue(x-1,y+1),get_blue(x,y+1),get_blue(x+1,y+1),get_blue(x+1,y),
          x, y,
          region_tracker_blue, region_tracker_limit, &region_tracker_blue_top,
          LZW_input+LZW_input_end,
          last_type_blue,
          f);
      //triples OR the last bit with 0x80 when they are done. We find those and increment input_len as appropriate.
      //LZW_input_begin and LZW_input_len aren't necessary aligned to 3 bytes, but their sum always is
      while((LZW_input[(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18) + 2] & 0x80) != 0) {
        LZW_input[(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18) + 2] &= 0x7f;
        LZW_input_len += 3;
      }
      //if we're testing, don't do LZW compression, just test to see if the initial encoder works
      if (TEST && (LZW_input_len > 0)) {
        printf("reallocating output buffer from size %u to %u\n",output_len,output_len + LZW_input_len + 6);
        if (output_index + LZW_input_len >= output_len) {
          output = realloc(output,output_len + LZW_input_len + 6);;
          output_len += LZW_input_len + 6;
        }
        printf("writing %u bytes to %p\n",min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin),output + output_index);
        memcpy(output + output_index,LZW_input + LZW_input_begin,min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin));
        if (LZW_input_len > region_tracker_limit*18 - LZW_input_begin) {
          printf("writing %u bytes to %p\n",(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18),output + output_index + region_tracker_limit*18 - LZW_input_begin);
          memcpy(output + output_index + region_tracker_limit*18 - LZW_input_begin,LZW_input,(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18));
        }
        LZW_input_begin = (LZW_input_begin + LZW_input_len) % (region_tracker_limit*18);
        printf("wrote %u bytes\n",LZW_input_len);
        output_index += LZW_input_len;
        LZW_input_len = 0;
      } else {
      //if we have anything to input, input it until we get a string not in the dict
      while(LZW_input_len > 0) {
        int new_prefix = next_prefix_index(
            LZW_dict, LZW_dict_nextindex,
            LZW_prefix_start, LZW_input[LZW_input_begin]);
        //current string is still in the dict, so increment LZW_input_begin and continue
        if (new_prefix >= 0) {
          LZW_input_begin = (LZW_input_begin + 1) % (region_tracker_limit*18);
          LZW_input_len--;
          LZW_prefix_start = new_prefix;
        } else {
          //current prefix + new postfix is not in the dict, so we output the current prefix and to not increment input
          LZW_output(
              LZW_prefix_start,LZW_input[LZW_input_begin],
              &output, &output_index, &output_bitoffset, &output_len, &LZW_index_bitwidth,
              LZW_dict, &LZW_dict_nextindex, dict_max_len);
          LZW_prefix_start = -1; //set prefix to be 0-length
        }
      }
      }
      //increment LZW_input_end to put a new triple in it
      LZW_input_end = (LZW_input_end + 3) % (region_tracker_limit*18);
    }
  }
  //deactivate every still-active region for every color in every row, and output all the resulting output
  for(int i = 0; i < region_tracker_limit; i++) {
    if(region_tracker_red[i].is_active) {
      region_tracker_red[i].output_loc[2] = region_tracker_red[i].l;
    }
    if(region_tracker_green[i].is_active) {
      region_tracker_green[i].output_loc[2] = region_tracker_green[i].l;
    }
    if(region_tracker_blue[i].is_active) {
      region_tracker_blue[i].output_loc[2] = region_tracker_blue[i].l;
    }
  }
  printf("deactivating and finalizing all regions\n");
  //we now have a guarantee that every triple from LZW_input_begin to LZW_input_end is finished, so we output them
  //LZW_input_len is inclusive, so we have to be careful
  LZW_input_len = LZW_input_begin < LZW_input_end ? LZW_input_end - LZW_input_begin : LZW_input_end + region_tracker_limit*18 - LZW_input_begin;
  if (TEST) {
    printf("reallocating output buffer from size %u to %u\n",output_len,output_len + LZW_input_len + 6);
    if (output_index + LZW_input_len >= output_len) {
      output = realloc(output,output_len + LZW_input_len + 6);;
      output_len += LZW_input_len + 6;
    }
    printf("writing %u bytes to %p\n",min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin),output + output_index);
    memcpy(output + output_index,LZW_input + LZW_input_begin,min(LZW_input_len,region_tracker_limit*18 - LZW_input_begin));
    if (LZW_input_len > region_tracker_limit*18 - LZW_input_begin) {
      printf("writing %u bytes to %p\n",(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18),output + output_index + region_tracker_limit*18 - LZW_input_begin);
      memcpy(output + output_index + region_tracker_limit*18 - LZW_input_begin,LZW_input,(LZW_input_begin + LZW_input_len) % (region_tracker_limit*18));
    }
    printf("wrote %u bytes\n",LZW_input_len);
  } else {
  while(LZW_input_len > 0) {
    int new_prefix = next_prefix_index(
        LZW_dict, LZW_dict_nextindex,
        LZW_prefix_start, LZW_input[LZW_input_begin]);
    //current string is still in the dict, so increment LZW_input_begin and continue
    if (new_prefix >= 0) {
      LZW_input_begin = (LZW_input_begin + 1) % (region_tracker_limit*18);
      LZW_input_len--;
      LZW_prefix_start = new_prefix;
    } else {
      //current prefix + new postfix is not in the dict, so we output the current prefix and to not increment input
      LZW_output(
          LZW_prefix_start,LZW_input[LZW_input_begin],
          &output, &output_index, &output_bitoffset, &output_len, &LZW_index_bitwidth,
          LZW_dict, &LZW_dict_nextindex, dict_max_len);
      LZW_prefix_start = -1; //set prefix to be 0-length
    }
  }
  }
  //free state
  free(region_tracker_red);
  free(region_tracker_blue);
  free(region_tracker_green);
  free(LZW_input);
  free(LZW_dict);
  *output_len1 = output_len;
  printf("used %d region slots \n",max(max(region_tracker_red_top,region_tracker_green_top),region_tracker_blue_top));
  return output;
}

//read a file, encode it, and output it to a file
//./encode [test] width height input output
//input == "stdin" -> read from stdin
//output == "stdout" -> output to stdout
int main(int argc, char** argv) {
  if(argc < 5) {
    return 1;
  }
  int test = 0;
  int rd_stdin = 0;
  int wr_stdout = 0;
  if (strncmp(argv[1],"test",4) == 0) {
    test = 1;
  }
  int width = atoi(argv[1+test]);
  int height = atoi(argv[2+test]);
  if (strncmp(argv[3+test],"stdin",5) == 0) {
    rd_stdin = 1;
  }
  if (strncmp(argv[4+test],"stdout",6) == 0) {
    wr_stdout = 1;
  }
  unsigned char* input = malloc(width*height*3);
  FILE* rd = rd_stdin ? stdin : fopen(argv[3+test],"r");
  FILE* wr = wr_stdout ? stdin : fopen(argv[4+test],"w");
  fread(input,1,width*height*3,rd);
  unsigned int output_len;
  unsigned char* output = encode_buffer(
      input,width,height,
      64*width, //for now just use the maximum necessary values, this will be tuneable later
      16, //also use the max bit width. Might not be tuneable tbh
      &default_f, //also use default f, will be tuneable later
      &output_len,
      test);
  printf("writing output");
  fwrite(output,1,output_len,wr);
  fflush(wr);
  fclose(wr);
  free(input);
  free(output);
  return 0;
}
