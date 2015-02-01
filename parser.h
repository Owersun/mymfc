#pragma once
/*
 * V4L2 Codec decoding example application
 * Kamil Debski <k.debski@samsung.com>
 *
 * Really simple stream parser header file
 *
 * Copyright 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

class Parser 
{
public:
  Parser();
  virtual ~Parser();
  
  int finished;

  /* H264 parser states */
  enum mfc_h264_parser_state {
    H264_PARSER_NO_CODE,
    H264_PARSER_CODE_0x1,
    H264_PARSER_CODE_0x2,
    H264_PARSER_CODE_0x3,
    H264_PARSER_CODE_1x1,
    H264_PARSER_CODE_SLICE,
  };

  /* H264 recent tag type */
  enum mfc_h264_tag_type {
    H264_TAG_HEAD,
    H264_TAG_SLICE,
  };

  /* MPEG4 parser states */
  enum mfc_mpeg4_parser_state {
    MPEG4_PARSER_NO_CODE,
    MPEG4_PARSER_CODE_0x1,
    MPEG4_PARSER_CODE_0x2,
    MPEG4_PARSER_CODE_1x1,
  };

  /* MPEG4 recent tag type */
  enum mfc_mpeg4_tag_type {
    MPEG4_TAG_HEAD,
    MPEG4_TAG_VOP,
  };

  /* Parser context */
  typedef struct mfc_parser_context {
    int state;
    int last_tag;
    char bytes[6];
    int main_count;
    int headers_count;
    int tmp_code_start;
    int code_start;
    int code_end;
    char got_start;
    char got_end;
    char seek_end;
    int short_header;
  } mfc_parser_context;

  mfc_parser_context ctx;

  /* Initialize the stream parser */
  int parse_stream_init(mfc_parser_context *ctx);

  virtual int parse_stream(mfc_parser_context *ctx,
          char* in, int in_size, unsigned char* out, int out_size,
          int *consumed, unsigned int *frame_size, char get_head);
};

class ParserMpeg4: public Parser
{
public:
  ParserMpeg4();
  virtual ~ParserMpeg4();
  int parse_stream(mfc_parser_context *ctx,
          char* in, int in_size, unsigned char* out, int out_size,
          int *consumed, unsigned int *frame_size, char get_head);
};
class ParserH264: public Parser
{
public:
  ParserH264();
  virtual ~ParserH264();
  int parse_stream(mfc_parser_context *ctx,
          char* in, int in_size, unsigned char* out, int out_size,
          int *consumed, unsigned int *frame_size, char get_head);
};
class ParserMpeg2: public Parser
{
public:
  ParserMpeg2();
  virtual ~ParserMpeg2();
  int parse_stream(mfc_parser_context *ctx,
          char* in, int in_size, unsigned char* out, int out_size,
          int *consumed, unsigned int *frame_size, char get_head);
};
