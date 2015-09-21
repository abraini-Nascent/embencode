/// @file
/// Embedded bencode support, header definitions.
// 2012-09-29 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#pragma once
#ifndef _EMBENCODE_h
#define _EMBENCODE_h

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/// Encoder class to generate Bencode on the fly (no buffer storage needed).
template <int bufLen>
class EmBencode {
public:
	uint8_t buffer[bufLen];
	uint8_t buffIdx = 0;

	EmBencode () {}
  
  /// Push a string out in Bencode format.
  /// @param str The zero-terminated string to send out (without trailing \0).
 void push (const char* str) {
    push(str, strlen(str));
  }

  /// Push arbitrary bytes in Bencode format.
  /// @param ptr Pointer to the data to send out.
  /// @param len Number of data bytes to send out.
 void push (const void* ptr, uint8_t len) {
    PushCount(len);
    PushChar(':');
    PushData(ptr, len);
  }

  /// Push a signed integer in Bencode format.
  /// @param val The integer to send (this implementation supports 32 bits).
void push (long val) {
    PushChar('i');
    if (val < 0) {
      PushChar('-');
      val = -val;
    }
    PushCount(val);
    PushEnd();
  }

   /// Push a zero interger in Bencode format.
void pushZero() {
	PushChar('i');
	PushChar('0');
	PushEnd();
}

  /// Start a new new list. Must be matched with a call to endList().
  /// Entries can be nested with more calls to startList(), startDict(), etc.
void startList () {
    PushChar('l');
  }

  /// Terminate a list, started earlier with a call to startList().
void endList () {
    PushEnd();
  }

  /// Start a new new dictionary. Must be matched with a call to endDict().
  /// Dictionary entries must consist of a string key plus an arbitrary value.
  /// Entries can be nested with more calls to startList(), startDict(), etc.
void startDict () {
    PushChar('d');
  }

  /// Terminate a dictionary, started earlier with a call to startDict().
void endDict () {
    PushEnd();
  }

void reset()
{
	memset(&buffer, 0, sizeof(buffer));
	buffIdx = 0;
}

protected:
void PushCount (uint32_t num) {
    char buf[11];
	ultoa(num, buf, 10);
    PushData(buf, strlen(buf));
  }

void PushEnd () {
    PushChar('e');
  }

void PushData (const void* ptr, uint8_t len) {
    for (const char* p = (const char*) ptr; len > 0; ++p, --len)
      PushChar(*p);
  }

  /// This function is not implemented in the library. It must be supplied by
  /// the caller to implement the actual writing of caharacters.
void PushChar(char ch)
{
	buffer[buffIdx] = ch;
	buffIdx++;
}

};

/// Decoder enum
enum { EMB_ANY, EMB_LEN, EMB_INT, EMB_STR };
enum { T_STRING = 0, T_NUMBER = 251, T_DICT, T_LIST, T_POP, T_END };
/// Decoder class, templated internal buffer to collect the incoming data.
template <int bufLen>
class EmBdecode {
protected:
	char level, buffer[bufLen];
	uint8_t count, next, last, state;

	void AddToBuf(char ch)
	{
		if (next >= bufLen)
			buffer[0] = T_END; // mark entire buffer as empty
		else
			buffer[next++] = ch;
	};

public:
  /// Types of tokens, as returned by nextToken().
  enum { T_STRING = 0, T_NUMBER = 251, T_DICT, T_LIST, T_POP, T_END };

  /// Initialize a decoder instance with the specified buffer space.
  /// @param buf Pointer to the buffer which will be used by the decoder.
  /// @param len Size of the buffer, must be in the range 50 to 255.
  EmBdecode()
  { 
	  reset(); 
  }

  /// Reset the decoder - can be called to prepare for a new round of decoding.
  uint8_t reset()
  {
	  count = next;
	  level = next = 0;
	  state = EMB_ANY;
	  return count;
  }

  /// Process a single incoming caharacter.
  /// @return Returns a count > 0 when the buffer contains a complete packet.
  uint8_t process(char ch){
	  switch (state) {
	  case EMB_ANY:
		  if (ch < '0' || ch > '9') {
			  if (ch == 'i') {
				  AddToBuf(T_NUMBER);
				  state = EMB_INT;
			  }
			  else if (ch == 'd' || ch == 'l') {
				  AddToBuf(ch == 'd' ? T_DICT : T_LIST);
				  ++level;
			  }
			  else if (ch == 'e') {
				  AddToBuf(T_POP);
				  --level;
				  break; // end of dict or list
			  }
			  return 0;
		  }
		  state = EMB_LEN;
		  count = 0;
		  // fall through
	  case EMB_LEN:
		  if (ch == ':') {
			  AddToBuf(T_STRING + count);
			  if (count == 0) {
				  AddToBuf(0);
				  break; // empty string
			  }
			  state = EMB_STR;
		  }
		  else
			  count = 10 * count + (ch - '0');
		  return 0;
	  case EMB_STR:
		  AddToBuf(ch);
		  if (--count == 0) {
			  AddToBuf(0);
			  break; // end of string
		  }
		  return 0;
	  case EMB_INT:
		  if (ch == 'e') {
			  AddToBuf(0);
			  break; // end of int
		  }
		  AddToBuf(ch);
		  return 0;
	  }
	  // end of an item reached
	  if (level > 0) {
		  state = EMB_ANY;
		  return 0;
	  }
	  AddToBuf(T_END);
	  return reset(); // not in dict or list, data is complete
  };

  /// Call this after process() is done, to extract each of the data tokens.
  /// @returns Returns one of the T_STRING .. T_END enumeration codes.
  uint8_t nextToken ()
  {
	  uint8_t ch = buffer[next++];
	  last = next;
	  switch (ch) {
	  default: // string
		  next += ch + 1;
		  return T_STRING;
	  case T_NUMBER:
		  while (buffer[next++] != 0)
			  ;
		  break;
	  case T_END:
		  --next; // don't advance past end token
		  // fall through
	  case T_DICT:
	  case T_LIST:
	  case T_POP:
		  break;
	  }
	  return ch;
  };

  /// Extract the last token as string (works for T_STRING and T_NUMBER).
  /// @param plen This variable will receive the size, if present.
  /// @return Returns pointer to a zero-terminated string in the decode buffer.
  const char* asString (uint8_t* plen =0)
  {
	  if (plen != 0)
		  *plen = next - last - 1;
	  return buffer + last;
  };

  /// Extract the last token as number (also works for strings if numeric).
  /// @return Returns the decoded integer, max 32-bit signed in this version.
  long asNumber ()
  {
	  return atol(buffer + last);
  };
};

#endif