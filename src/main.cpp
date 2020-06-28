#include <vector>
#include <map>
#include <iostream>
#include <utility>
#include <cstdlib>

using namespace std;

//output some packed bits
//the "bits" input must ONLY have bits set in the first num_bits bits.
void output_bits(unsigned char bits, vector<unsigned char>& output, int* bit_offset, unsigned char* current_char, int num_bits) {
  *current_char |= (bits >> *bit_offset);
  if (*bit_offset + num_bits >= 8) {
    output.push_back(*current_char);
    *current_char = bits << (8 - *bit_offset);
    *bit_offset = num_bits + *bit_offset - 8;
  } else {
    *bit_offset += num_bits;
  }
}
//special LZW that is aware of the pixel encoding we use.
//each unit is exactly one code-point.
/*vector<unsigned char> slzw(vector<unsigned char>& v, int base) {
  vector<unsigned char> ret = vector<unsigned char>();
  ret.insert(ret.end(),v.begin(),v.begin()+base); //the first part is unchanged
  vector<pair<unsigned char, int>> dict;

}*/

//packed ints are optimized for storing small numbers, specifically 0 and +-1. They cannot store integers larger than +-17
//0 and +-1 are encoded in 2 bits, [2,17] and [-17,-2] are encoded in 7 bits.

//encode a packed int into an unsigned char
//return value is pair(the value encoded into an int, number of bits to advance
pair<unsigned char, int> encode_packed_int(int val) {
  if (val == 0) {
    return pair<unsigned char, int>(0x00,2);
  } else if (val == 1) {
    return pair<unsigned char, int>(0x40,2);
  } else if (val == -1) {
    return pair<unsigned char, int>(0x80,2);
  } else if ((-17 <= val) && (val < 0)) {
    unsigned char magnitude_encoded = (val*-1)-2;
    return pair<unsigned char, int>(0xe0 | (magnitude_encoded << 1),7);
  } else if (val <= 17) {
    return pair<unsigned char, int>(0xc0 | ((val - 2) << 1),7);
  }
  return pair<unsigned char, int>(0xff, 0);
}

//decode a packed int starting at char char_offset and bit bit_offset, in vector v
//return value is (val,bit_advance) where val is the decoded value and bit_advance is the number of bits used to encode the value
pair<int, int> decode_packed_int(unsigned char v1) {
  int val = 0;
  int bit_advance = 0;
  switch (v1 & 0xc0) { //first two bits tell us exactly what kind of integer this is
    case 0xc0: //11 => this is a 5-bit integer with a sign bit. The magnitude encoded is abs(k-2). So this can encode +-k with k in [2,17]
      {
        int sign = ((v1 & 0x10) == 0) ? 1 : -1;
        val = sign*((((unsigned int)v1 & 0xe) >> 1) + 2);
        bit_advance = 8;
        break;
      }
    case 0x80: //10 -> -1
      {
        val = -1;
        bit_advance = 2;
        break;
      }
    case 0x40: //01 -> 1
      {
        val = 1;
        bit_advance = 2;
        break;
      }
    case 0x00: //00 -> 0
      {
        val = 0;
        bit_advance = 2;
        break;
      }
    default: break;
  };
  return pair<int,int>(val,bit_advance);
}

//compression scheme is similar to PNG, but with the assumption that most regions will be made of contiguous color. 
//Therefore, the compression is designed specifically to optimize for the case when there is an adjacent pixel to
//the current pixel which is the same color or very close, or that the adjacent colors are dissimilar, and there are
//few colors overall
//color points are encoded like so:
//first bit 0 -> next 2 bits are a direction code. The pixel in that direction is identical to this one.
//first bit 1 -> next 2-7 bits refer to an element of the color table, in packed-int form. Some color table indices have
//               special meaning:
//               -17 -> the next byte is a *full-size* index into the color table (it contains N - 32)
//               -16 -> this color does not occur in the color table and the next n bytes are unsigned 1-byte deltas from the top pixel
//               -15 -> this color does not occur in the color table and the next n bytes are unsigned 1-byte deltas from the left pixel
//               actual indices are encoded such that -1 -> 0, 0 -> 1, 1 -> 2, +N -> N+2, -N -> N+18 (for N >= 2)
//
//after that is a number which says how many times this pixel is repeated(not crossing a row boundary). The width is chosen to align to byte boundaries.
//
//direction codes:
//00 -> left
//01 -> upper left
//10 -> top
//11 -> upper right
//

enum Direction {
  left,
  up_left,
  up,
  up_right,
  none,
};

//checks for a nearby direction which is identical to the given pixel
//immediately left has priority, then the three above pixels
Direction get_identical_direction_rgb(vector<unsigned char>& raw, int i, int j, int rowlen) { 
  unsigned char r = raw[3*(i*rowlen + j)];
  unsigned char g = raw[3*(i*rowlen + j) + 1];
  unsigned char b = raw[3*(i*rowlen + j) + 2];
  if (j > 0) {
    if ((r == raw[3*(i*rowlen + j) - 3]) && (g == raw[3*(i*rowlen + j) - 2]) && (b == raw[3*(i*rowlen + j) - 1])) {
      return Direction::left;
    }
  }
  if (i > 0) {
    if ((r == raw[3*((i-1)*rowlen + j)]) && (g == raw[3*((i-1)*rowlen + j) + 1]) && (b == raw[3*((i-1)*rowlen + j) + 2])) {
      return Direction::up;
    } else if ((j > 0) && (r == raw[3*((i-1)*rowlen + j) - 3]) && (g == raw[3*((i-1)*rowlen + j) - 2]) && (b == raw[3*((i-1)*rowlen + j) - 1])) {
      return Direction::up_left;
    } else if ((j < (rowlen-1)) && (r == raw[3*((i-1)*rowlen + j) + 3]) && (g == raw[3*((i-1)*rowlen + j) + 4]) && (b == raw[3*((i-1)*rowlen + j) + 5])) {
      return Direction::up_right;
    }
  }
  return Direction::none;
}

//get the deltas from a nearby pixel in the given direction
unsigned char get_deltas(vector<unsigned char>& raw, int i, int j, int rowlen, Direction d) {
  unsigned char r = raw[3*(i*rowlen + j)];
  unsigned char g = raw[3*(i*rowlen + j) + 1];
  unsigned char b = raw[3*(i*rowlen + j) + 2];
  switch (d) {
    case Direction::left: {
      if (j > 0) {
        unsigned char r_diff = r - raw[3*(i*rowlen + j - 1)];
        unsigned char g_diff = g - raw[3*(i*rowlen + j - 1) + 1];
        unsigned char b_diff = b - raw[3*(i*rowlen + j - 1) + 2];
        return (((unsigned int)r_diff) << 16) | (((unsigned int)g_diff) << 8) | b_diff;
      } else {
        return 0;
      }
    }
    case Direction::up_left: {
      if ((i > 0) && (j > 0)) {
        unsigned char r_diff = r - raw[3*((i-1)*rowlen + j - 1)];
        unsigned char g_diff = g - raw[3*((i-1)*rowlen + j - 1) + 1];
        unsigned char b_diff = b - raw[3*((i-1)*rowlen + j - 1) + 2];
        return (((unsigned int)r_diff) << 16) | (((unsigned int)g_diff) << 8) | b_diff;
      } else {
        return 0;
      }
    }
    case Direction::up: {
      if (i > 0) {
        unsigned char r_diff = r - raw[3*((i-1)*rowlen + j)];
        unsigned char g_diff = g - raw[3*((i-1)*rowlen + j) + 1];
        unsigned char b_diff = b - raw[3*((i-1)*rowlen + j) + 2];
        return (((unsigned int)r_diff) << 16) | (((unsigned int)g_diff) << 8) | b_diff;
      } else {
        return 0;
      }
    }
    case Direction::up_right: {
      if ((i > 0) && (j < (rowlen - 1))) {
        unsigned char r_diff = r - raw[3*((i-1)*rowlen + j + 1)];
        unsigned char g_diff = g - raw[3*((i-1)*rowlen + j + 1) + 1];
        unsigned char b_diff = b - raw[3*((i-1)*rowlen + j + 1) + 2];
        return (((unsigned int)r_diff) << 16) | (((unsigned int)g_diff) << 8) | b_diff;
      } else {
        return 0;
      }
    }
    default: return 0;
  }
}

//get the index that this color occupies in the color table
//does the wacky conversion too
int get_color_index(unsigned char r, unsigned char g, unsigned char b, vector<unsigned int>& color_table) {
  unsigned int color = (((unsigned int)r) << 16) | (((unsigned int)g) << 8) | b;
  for(int i = 0; i < color_table.size(); i++) {
    if (color_table[i] == color) {
      if (i < 3) {
        return i - 1;
      } else if(i < 18) {
        return i - 2;
      } else if(i < 33) {
        return -1*(i - 18);
      } else {
        return i; //just return i to avoid confusion
      }
    }
  }
  return 1000;
}

//gets rid of the wacky conversion of packed indexes into the color table
int convert_index_from_packed(unsigned char packed) {
  pair<int,int> unpacked = decode_packed_int(packed);
  if (unpacked.second == 2) {
    return unpacked.first + 1;
  } else if (unpacked.first > 0) {
    return unpacked.first + 1;
  } else if (unpacked.first > -15) {
    return (-1)*unpacked.first + 18;
  } else {
    return unpacked.first; //do not perform conversion for special indices
  }
}

//find the average (between r,g,and b) between a pixel and its neighbor
//we just support up and left for now, maybe more later
unsigned int get_average_distance(vector<unsigned char>& raw, int i, int j, int rowlen, Direction d) {
  unsigned char r = raw[3*(i*rowlen + j)];
  unsigned char g = raw[3*(i*rowlen + j) + 1];
  unsigned char b = raw[3*(i*rowlen + j) + 2];
  switch (d) {
    case Direction::up: {
      if (i > 0) {
        unsigned char r_diff = r - raw[3*((i-1)*rowlen + j)];
        unsigned char g_diff = g - raw[3*((i-1)*rowlen + j) + 1];
        unsigned char b_diff = b - raw[3*((i-1)*rowlen + j) + 2];
        return (((unsigned int)r_diff) + ((unsigned int)g_diff) + b_diff)/3;
      } else {
        return 1000; //impossibly large number if there is no pixel in that direction
      }
    }
    case Direction::left: {
      if (j > 0) {
        unsigned char r_diff = r - raw[3*(i*rowlen + j - 1)];
        unsigned char g_diff = g - raw[3*(i*rowlen + j - 1) + 1];
        unsigned char b_diff = b - raw[3*(i*rowlen + j - 1) + 2];
        return (((unsigned int)r_diff) + ((unsigned int)g_diff) + b_diff)/3;
      } else {
        return 1000; //impossibly large number if there is no pixel in that direction
      }
    }
    default: return 1000;
  }
}

//encodes a vector of 3-byte RGB triples in the above scheme
vector<unsigned char> encode_rgb(vector<unsigned char> raw, int rowlen) {
  //calculate which colors should be stored in the color table.
  //heuristically, we want the colors which
  // -show up often at boundaries
  // -when they show up, aren't always close to an identical color (generally, it's most efficient to encode that way)
  //so, for each color, we count the number of times x it shows up at a boundary, and subtract
  //the number of times y that it can be encoded as a nearby color.
  map<unsigned int, int> color_counts = map<unsigned int, int>();
  for (unsigned int i = 0; i < raw.size()/(3*rowlen); i++) {
    for (int j = 0; j < rowlen; j++) {
      //check to see if (r,g,b) matches any of the immediately nearby pixels
      if (get_identical_direction_rgb(raw, i, j, rowlen) != none) {
        continue; //do nothing, this will be encoded as a nearby pixel
      } else {
        unsigned char r = raw[3*(i*rowlen + j)];
        unsigned char g = raw[3*(i*rowlen + j) + 1];
        unsigned char b = raw[3*(i*rowlen + j) + 2];
        unsigned int rgb = (((unsigned int)r) << 16) | (((unsigned int)g) << 8) | b;
        //if this color is being counted, add it to the counts or increment its counter
        if (color_counts.count(rgb) > 0) {
          color_counts[rgb] ++;
        } else {
          color_counts[rgb] = 1;
        }
      }
    }
  }
  //sort the colors by frequency
  vector<pair<unsigned int, int>> counts_sorted = vector<pair<unsigned int, int>>();
  counts_sorted.reserve(color_counts.size()); //make sure to use the right capacity, so that the iterators are never invalidated
  for (auto it = color_counts.begin(); it != color_counts.end(); it++) {
    for (auto vec_it = counts_sorted.begin(); vec_it != counts_sorted.end(); vec_it++) {
      if (vec_it->second >= it->second) {
        counts_sorted.insert(vec_it,pair<unsigned int, int>(it->first,it->second));
      }
    }
  }
  //create the color table
  vector<unsigned int> color_table = vector<unsigned int>();
  for (int i = 0; i < 287; i++) {
    color_table.push_back(counts_sorted[i].first);
  }
  //the color table must include the top left color, otherwise the image can't decode
  color_table.push_back((((unsigned int)raw[0]) << 16) | (((unsigned int)raw[1]) << 8) | raw[2]);
  //now start encoding in ernest
  vector<unsigned char> output = vector<unsigned char>();
  //output the magic number
  output.push_back('b');
  output.push_back('o');
  output.push_back('o');
  //output the row length
  output.push_back((unsigned char)(rowlen >> 24));
  output.push_back((unsigned char)(rowlen >> 16));
  output.push_back((unsigned char)(rowlen >> 8));
  output.push_back((unsigned char)(rowlen));
  //output the color table
  output.push_back(((unsigned char)color_table.size()) >> 8);
  output.push_back((unsigned char)color_table.size());
  for (unsigned int i = 0; i < color_table.size(); i++) {
    output.push_back((unsigned char)(color_table[i] >> 16));
    output.push_back((unsigned char)(color_table[i] >> 8));
    output.push_back((unsigned char)color_table[i]);
  }
  //start outputting color points
  int bit_offset = 0;
  unsigned char current_byte;
  for (unsigned int i = 0; i < raw.size()/(3*rowlen); i++) {
    //start encoding the row
    unsigned char counter = 0;
    for(int j = 0; j < rowlen; j++) {
      Direction is_nearby = get_identical_direction_rgb(raw,i,j,rowlen);
      //case: same color as last pixel and counter isn't full
      if ((is_nearby == Direction::left) && (counter != 0)) {
        counter++;
        if (counter == 255) {
          output_bits(counter,output,&bit_offset,&current_byte,8);
          counter = 0;
        }
      //case: same color as some nearby pixel
      } else if (is_nearby == Direction::left) {
        output_bits(0x00,output,&bit_offset,&current_byte,3);
        counter = 1;
      } else if (is_nearby == Direction::up_left) {
        output_bits(0x20,output,&bit_offset,&current_byte,3);
        counter = 1;
      } else if (is_nearby == Direction::up) {
        output_bits(0x40,output,&bit_offset,&current_byte,3);
        counter = 1;
      } else if (is_nearby == Direction::up_right) {
        output_bits(0x60,output,&bit_offset,&current_byte,3);
        counter = 1;
      //case: not the same color as any nearby pixel, but in the color table
      } else {
        output_bits(0x80,output,&bit_offset,&current_byte,1);
        unsigned char r = raw[3*(i*rowlen + j)];
        unsigned char g = raw[3*(i*rowlen + j) + 1];
        unsigned char b = raw[3*(i*rowlen + j) + 2];
        int index = get_color_index(r,g,b,color_table);
        if (index < 18) { //less than 18 -> shortpacked
          pair<unsigned char,int> index_encoded = encode_packed_int(index);
          output_bits(index_encoded.first,output,&bit_offset,&current_byte,index_encoded.second);
          counter = 1;
        } else if (index < 1000) { //less than 1000 -> is in the table
          output_bits(0xfe,output,&bit_offset,&current_byte,7); //output 11 1 1111 = -17
          output_bits((unsigned char)(index - 32),output,&bit_offset,&current_byte,8);
          counter = 1;
        //case: not in color table, use delta encoding
        } else {
          //take the direction with the minimum average distance among the different colors
          int avgdist_up = get_average_distance(raw,i,j,rowlen,Direction::up);
          int avgdist_left = get_average_distance(raw,i,j,rowlen,Direction::left);
          if (avgdist_up < avgdist_left) {
            output_bits(0xfc,output,&bit_offset,&current_byte,7); //output 11 1 1110 -> -16
            output_bits((unsigned char)(r - raw[3*((i-1)*rowlen + j)]),output,&bit_offset,&current_byte,8);
            output_bits((unsigned char)(g - raw[3*((i-1)*rowlen + j) + 1]),output,&bit_offset,&current_byte,8);
            output_bits((unsigned char)(b - raw[3*((i-1)*rowlen + j) + 2]),output,&bit_offset,&current_byte,8);
            counter = 1;
          } else {
            output_bits(0xfa,output,&bit_offset,&current_byte,7); //output 11 1 1101 -> -15
            output_bits((unsigned char)(r - raw[3*(i*rowlen + j - 1)]),output,&bit_offset,&current_byte,8);
            output_bits((unsigned char)(g - raw[3*(i*rowlen + j - 1) + 1]),output,&bit_offset,&current_byte,8);
            output_bits((unsigned char)(b - raw[3*(i*rowlen + j - 1) + 2]),output,&bit_offset,&current_byte,8);
            counter = 1;
          }
        }
      }
    }
  }
  //perform SLZW which is just LZW but aware of our varying color point sizes(it has 3-bit and 8-bit datums)
  //return slzw(output,9 + 3*color_table.size()); for now we just want to test the encoder, LZW can come later
  return output;
}

//reads bits from a byte vector
unsigned char read_bits(vector<unsigned char>& v, int* current_index, int* bit_offset, int num_bits) {
  unsigned char ret = (v[*current_index] << *bit_offset) & (0xff << (8 - num_bits));
  if (*bit_offset + num_bits >= 8) {
    *current_index = *current_index + 1;
    ret |= v[*current_index] >> (16 - num_bits - *bit_offset);
    *bit_offset = *bit_offset + num_bits - 8;
  } else {
    *bit_offset = *bit_offset + num_bits;
  }
  return ret;
}

//decoding. Much simpler than encoding.
//TODO: optimized decoder!
vector<unsigned char> decode_rgb(vector<unsigned char> encoded) {
  //check for the magic bytes
  for(int i = 0; i < 3; i++) {
    if (encoded[i] != "boo"[i]) {
      return vector<unsigned char>();
    }
  }
  int rowlen = 0;
  //read the row length
  for(int i = 3; i < 7; i++) {
    rowlen += ((int)encoded[i]) << (8*(7-i+1));
  }
  //read the color table
  vector<unsigned int> color_table = vector<unsigned int>();
  int color_table_len = (((unsigned int)(encoded[7])) << 8) + encoded[8];
  for(int i = 9; i < 9+3*color_table_len; i+= 3) {
    color_table.push_back((((unsigned int)encoded[i]) << 16) | (((unsigned int)encoded[i+1]) << 8) | encoded[i+2]);
  }
  int offset = 9+3*color_table_len;
  vector<unsigned char> raw = vector<unsigned char>();
  //start reading code points
  int i = 0;
  int j = 0;
  int bit_offset = 0;
  for (int p = offset; p < encoded.size();) {
    //read 3 bits to see what kind of code this is
    unsigned char code = read_bits(encoded,&p,&bit_offset,3);
    switch(code) {
      //handle direction codes
      case 0: { //left
        raw.push_back(raw[3*(i*rowlen + j - 1)]);
        raw.push_back(raw[3*(i*rowlen + j - 1) + 1]);
        raw.push_back(raw[3*(i*rowlen + j - 1) + 2]);
        break;
      }
      case 0x20: { //up_left
        raw.push_back(raw[3*((i-1)*rowlen + j - 1)]);
        raw.push_back(raw[3*((i-1)*rowlen + j - 1) + 1]);
        raw.push_back(raw[3*((i-1)*rowlen + j - 1) + 2]);
        break;
      }
      case 0x40: { //up
        raw.push_back(raw[3*((i-1)*rowlen + j)]);
        raw.push_back(raw[3*((i-1)*rowlen + j) + 1]);
        raw.push_back(raw[3*((i-1)*rowlen + j) + 2]);
        break;
      }
      case 0x60: { //up_right
        raw.push_back(raw[3*((i-1)*rowlen + j + 1)]);
        raw.push_back(raw[3*((i-1)*rowlen + j + 1) + 1]);
        raw.push_back(raw[3*((i-1)*rowlen + j + 1) + 2]);
        break;
      }
      //handle small packed-int indices to the color table
      case 0x80: { //index = 1
        raw.push_back((unsigned char)(color_table[1] >> 16));
        raw.push_back((unsigned char)(color_table[1] >> 8));
        raw.push_back((unsigned char)(color_table[1]));
        break;
      }
      case 0xa0: { //index = 2
        raw.push_back((unsigned char)(color_table[2] >> 16));
        raw.push_back((unsigned char)(color_table[2] >> 8));
        raw.push_back((unsigned char)(color_table[2]));
        break;
      }
      case 0xc0: { //index = 0
        raw.push_back((unsigned char)(color_table[0] >> 16));
        raw.push_back((unsigned char)(color_table[0] >> 8));
        raw.push_back((unsigned char)(color_table[0]));
        break;
      }
      case 0xe0: { //index is a 7-bit packed int, more processing needed
        unsigned char index_next5bits = read_bits(encoded,&p,&bit_offset,5);
        unsigned char index_packed = (code << 1) | (index_next5bits >> 2);
        int index = convert_index_from_packed(index_packed);
        if (index >= 0) {
          raw.push_back((unsigned char)(color_table[index] >> 16));
          raw.push_back((unsigned char)(color_table[index] >> 8));
          raw.push_back((unsigned char)(color_table[index]));
        } else if (index == -17) { //next byte is a full-size index giving N - 32
          index = (int)read_bits(encoded,&p,&bit_offset,8) + 32;
          raw.push_back((unsigned char)(color_table[index] >> 16));
          raw.push_back((unsigned char)(color_table[index] >> 8));
          raw.push_back((unsigned char)(color_table[index]));
        } else if (index == -16) { //next 3 bytes are deltas from the above pixel
          raw.push_back(raw[3*((i-1)*rowlen + j) + read_bits(encoded,&p,&bit_offset,8)]);
          raw.push_back(raw[3*((i-1)*rowlen + j) + 1] + read_bits(encoded,&p,&bit_offset,8));
          raw.push_back(raw[3*((i-1)*rowlen + j) + 2] + read_bits(encoded,&p,&bit_offset,8));
        } else if (index == -15) {//next 3 bytes are deltas from the previous pixel
          raw.push_back(raw[3*(i*rowlen + j - 1) + read_bits(encoded,&p,&bit_offset,8)]);
          raw.push_back(raw[3*(i*rowlen + j - 1) + 1] + read_bits(encoded,&p,&bit_offset,8));
          raw.push_back(raw[3*(i*rowlen + j - 1) + 2] + read_bits(encoded,&p,&bit_offset,8));
        }
        break;
      }
      default: break; //not possible
    }
    //now read the count
    //3*(i*rowlen + j) should be the bytes that were just outputted
    unsigned char count = read_bits(encoded,&p,&bit_offset,8);
    for(unsigned char ctr = 1; ctr < count; ctr++) {
      raw.push_back(raw[3*(i*rowlen + j)]);
      raw.push_back(raw[3*(i*rowlen + j) + 1]);
      raw.push_back(raw[3*(i*rowlen + j) + 2]);
    }
    //increment j and i as necessary
    j += count;
    if (j >= rowlen) {
      i++;
      j = 0;
    }
    //done!
  }
  return raw;
}

int main(int argc, char** argv) {
  //see whether we're encoding or decoding
  if (argv[1][0] == 'e') {
    //get the row length
    int rowlen = atoi(argv[2]);
    vector<unsigned char> raw_input = vector<unsigned char>();
    while(!cin.eof()) {
      raw_input.push_back(cin.get());
    }
    vector<unsigned char> encoded = encode_rgb(raw_input,rowlen);
    for (int i = 0; i < encoded.size(); i++) {
      cout.put(encoded[i]);
    }
  } else {
    vector<unsigned char> encoded_input = vector<unsigned char>();
    while(!cin.eof()) {
      encoded_input.push_back(cin.get());
    }
    vector<unsigned char> raw = decode_rgb(encoded_input);
    for (int i = 0; i < raw.size(); i++) {
      cout.put(raw[i]);
    }
  }
  return 0;
}


