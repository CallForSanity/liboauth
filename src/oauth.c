/*
 * oAuth string functions in POSIX-C.
 *
 * Copyright LGPL 2007 Robin Gareus <robin@mediamatic.nl>
 * 
 * The base64 functions are by Jan-Henrik Haukeland, <hauk@tildeslash.com>
 * and escape_url() was inspired by libcurl's curl_escape.
 * many thanks to Daniel Stenberg <daniel@haxx.se>.
 *
 *  This package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This package is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this package; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */
/* vi: sts=2 sw=2 ts=2 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <openssl/hmac.h>

#include "xmalloc.h"
#include "oauth.h"

/**
 * Base64 encode one byte
 */
char oauth_b64_encode(unsigned char u) {
  if(u < 26)  return 'A'+u;
  if(u < 52)  return 'a'+(u-26);
  if(u < 62)  return '0'+(u-52);
  if(u == 62) return '+';
  return '/';
}

/**
 * Decode a single base64 character.
 */
unsigned char oauth_b64_decode(char c) {
  if(c >= 'A' && c <= 'Z') return(c - 'A');
  if(c >= 'a' && c <= 'z') return(c - 'a' + 26);
  if(c >= '0' && c <= '9') return(c - '0' + 52);
  if(c == '+')             return 62;
  return 63;
}

/**
 * Return TRUE if 'c' is a valid base64 character, otherwise FALSE
 */
int oauth_b64_is_base64(char c) {
  if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
     (c >= '0' && c <= '9') || (c == '+')             ||
     (c == '/')             || (c == '=')) {
    return 1;
  }
  return 0;
}

/**
 * Base64 encode and return size data in 'src'. The caller must free the
 * returned string.
 *
 * @param size The size of the data in src
 * @param src The data to be base64 encode
 * @return encoded string otherwise NULL
 */
char *oauth_encode_base64(int size, unsigned char *src) {
  int i;
  char *out, *p;

  if(!src) return NULL;
  if(!size) size= strlen((char *)src);
  out= (char*) xcalloc(sizeof(char), size*4/3+4);
  p= out;

  for(i=0; i<size; i+=3) {
    unsigned char b1=0, b2=0, b3=0, b4=0, b5=0, b6=0, b7=0;
    b1= src[i];
    if(i+1<size) b2= src[i+1];
    if(i+2<size) b3= src[i+2];
      
    b4= b1>>2;
    b5= ((b1&0x3)<<4)|(b2>>4);
    b6= ((b2&0xf)<<2)|(b3>>6);
    b7= b3&0x3f;
      
    *p++= oauth_b64_encode(b4);
    *p++= oauth_b64_encode(b5);
      
    if(i+1<size) *p++= oauth_b64_encode(b6);
    else *p++= '=';
      
    if(i+2<size) *p++= oauth_b64_encode(b7);
    else *p++= '=';
  }
  return out;
}

/**
 * Decode the base64 encoded string 'src' into the memory pointed to by
 * 'dest'. 
 *
 * @param dest Pointer to memory for holding the decoded string.
 * Must be large enough to recieve the decoded string.
 * @param src A base64 encoded string.
 * @return the length of the decoded string if decode
 * succeeded otherwise 0.
 */
int oauth_decode_base64(unsigned char *dest, const char *src) {
  if(src && *src) {
    unsigned char *p= dest;
    int k, l= strlen(src)+1;
    unsigned char *buf= (unsigned char*) xcalloc(sizeof(unsigned char), l);

    /* Ignore non base64 chars as per the POSIX standard */
    for(k=0, l=0; src[k]; k++) {
      if(oauth_b64_is_base64(src[k])) {
        buf[l++]= src[k];
      }
    } 
    
    for(k=0; k<l; k+=4) {
      char c1='A', c2='A', c3='A', c4='A';
      unsigned char b1=0, b2=0, b3=0, b4=0;
      c1= buf[k];

      if(k+1<l) c2= buf[k+1];
      if(k+2<l) c3= buf[k+2];
      if(k+3<l) c4= buf[k+3];
      
      b1= oauth_b64_decode(c1);
      b2= oauth_b64_decode(c2);
      b3= oauth_b64_decode(c3);
      b4= oauth_b64_decode(c4);
      
      *p++=((b1<<2)|(b2>>4) );
      
      if(c3 != '=') *p++=(((b2&0xf)<<4)|(b3>>2) );
      if(c4 != '=') *p++=(((b3&0x3)<<6)|b4 );
    }
    free(buf);
    dest[p-dest]='\0';
    return(p-dest);
  }
  return 0;
}

/**
 * Escape 'string' according to RFC3986 and
 * http://oauth.googlecode.com/svn/spec/branches/1.0/drafts/7/spec.html#encoding_parameters
 *
 * @param string The data to be encoded
 * @return encoded string otherwise NULL
 * The caller must free the returned string.
 */
char *url_escape(const char *string) {
  if (!string) return strdup(""); // doc-check
  size_t alloc = strlen(string)+1;
  char *ns = NULL, *testing_ptr = NULL;
  unsigned char in; 
  size_t newlen = alloc;
  int strindex=0;
  size_t length;

  ns = (char*) xmalloc(alloc);

  length = alloc-1;
  while(length--) {
    in = *string;

    switch(in){
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '_': case '~': case '.': case '-':
      ns[strindex++]=in;
      break;
    default:
      newlen += 2; /* this'll become a %XX */
      if(newlen > alloc) {
        alloc *= 2;
	testing_ptr = (char*) xrealloc(ns, alloc);
	ns = testing_ptr;
      }
      snprintf(&ns[strindex], 4, "%%%02X", in);
      strindex+=3;
      break;
    }
    string++;
  }
  ns[strindex]=0;
  return ns;
}

/**
 * returns base64 encoded HMAC-SHA1 signature for
 * given message and key.
 * both data and key need to be urlencoded.
 *
 * the returned string needs to be freed by the caller
 *
 * @param m message to be signed
 * @param k key used for signing
 * @return signature string.
 */
char *oauth_sign_hmac_sha1 (char *m, char *k) {
  unsigned char result[EVP_MAX_MD_SIZE];
  unsigned int resultlen = 0;
  
  HMAC(EVP_sha1(), k, strlen(k), 
      (unsigned char*) m,strlen(m),
      result, &resultlen);

  return(oauth_encode_base64(resultlen, result));
}

/**
 * returns plaintext signature for the given key.
 *
 * the returned string needs to be freed by the caller
 *
 * @param m message to be signed
 * @param k key used for signing
 * @return signature string
 */
char *oauth_sign_plaintext (char *m, char *k) {
  return(xstrdup(k));
}

/**
 * returns RSA signature for given data.
 * data needs to be urlencoded.
 *
 * THIS FUNCTION IS NOT YET IMPLEMENTED!
 *
 * the returned string needs to be freed by the caller.
 *
 * @param m message to be signed
 * @param k key used for signing
 * @return signature string.
 */
#if 0

#include <openssl/rsa.h>
#include <openssl/engine.h>

char *oauth_sign_rsa_sha1 (char *m, char *k) {
  unsigned char *sig;
  unsigned int len;
  RSA *rsa = RSA_new();
  //TODO read RSA secret key (from file? - k?)
  //
  //NOTE:
  // oAuth requires PKCS #1 v2.1 [RFC 3447].
  // openssl-0.9.8g supports PKCS #1 v2.0 [RFC 2437] - see
  // `man 3 rsa` - maybe just the manpage is out of date?!
  // they refer to the 3447 RFC in the source doc.
  //
  if (!RSA_sign(NID_sha, (unsigned char *)m, strlen(m), sig, &len, rsa)) {
    printf("rsa signing failed\n");
  }
  RSA_free(rsa);
  return((char*) sig);
}
#else 
  char *oauth_sign_rsa_sha1 (char *m, char *k) {
   return xstrdup("RSA-is-not-implemented.");
  }
#endif

/**
 * encode strings and concatenate with '&' separator.
 * The number of strings to be concatenated must be
 * given as first argument.
 * all arguments thereafter must be of type (char *) 
 *
 * @param len the number of arguments to follow this parameter
 * @param ... string to escape and added
 *
 * @return pointer to memory holding the concatenated 
 * strings - needs to be free(d) by the caller. or NULL
 * in case we ran out of memory.
 */
char *catenc(int len, ...) {
  va_list va;
  char *rv = (char*) xmalloc(sizeof(char));
  *rv='\0';
  va_start(va, len);
  int i;
  for(i=0;i<len;i++) {
    char *arg = va_arg(va, char *);
    char *enc;
    int len;
    enc = url_escape(arg);
    if(!enc) break;
    len = strlen(enc) + 1 + ((i>0)?1:0);
    if(rv) len+=strlen(rv);
    rv=(char*) xrealloc(rv,len*sizeof(char));

    if(i>0) strcat(rv, "&");
    strcat(rv, enc);
    free(enc);
  }
  va_end(va);
  return(rv);
}

/**
 *
 */
int split_url_parameters(const char *url, char ***argv)
{
  int argc=0;
  char *token, *tmp, *t1;
  //(*argv) = NULL; //safety?! or free first?!

  t1=tmp=xstrdup(url);
  while((token=strtok(tmp,"&?"))) {
    if(!strncasecmp("oauth_signature",token,15)) continue;
    (*argv)=(char**) xrealloc(*argv,sizeof(char*)*(argc+1));
    (*argv)[argc]=xstrdup(token);
    tmp=NULL;
    argc++;
  }
  free(t1);
  return argc;
}

/**
 *
 */
char *serialize_url_parameters (int argc, char **argv) {
  char *token, *tmp, *t1;
  int i;
  char *query = (char*) xmalloc(sizeof(char)); 
  *query='\0';
  for(i=1; i< argc; i++) {
    int len = 0;
    if(query) len+=strlen(query);
    // see http://oauth.net/core/1.0/#encoding_parameters
    // escape parameter names and arguments but not the '='
    if(!(t1=strchr(argv[i], '='))) {
      tmp=xstrdup(argv[i]);
      len+=strlen(argv[i])+2;
    } else {
      *t1=0;
      tmp = url_escape(argv[i]);
      *t1='=';
      t1 = url_escape((t1+1));
      tmp=(char*) xrealloc(tmp,(strlen(tmp)+strlen(t1)+2)*sizeof(char));
      strcat(tmp,"=");
      strcat(tmp,t1);
      free(t1);
      len+=strlen(tmp)+2;
    }
    query=(char*) xrealloc(query,len*sizeof(char));
    strcat(query, (i==1?"":"&"));
    strcat(query, tmp);
    free(tmp);
  }
  return (query);
}

/**
 * generate a random string between 15 and 32 chars length
 * and return a pointer to it. The value needs to be freed by the
 * caller
 *
 * @return zero terminated random string.
 */
char *gen_nonce() {
  char *nc;
  static int rndinit = 1;
  const char *chars = "abcdefghijklmnopqrstuvwxyz"
  	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" "0123456789_";
  unsigned int max = strlen( chars );
  int i, len;

  if(rndinit) {srand(time(NULL)); rndinit=0;} // seed random number generator - FIXME: we can do better ;)

  len=15+floor(rand()*16.0/(double)RAND_MAX);
  nc = xmalloc((len+1)*sizeof(char));
  for(i=0;i<len; i++) {
    nc[i] = chars[ rand() % max ];
  }
  nc[i]='\0';
  return (nc);
}

/**
 * string compare function.
 *
 * used with qsort. needed to normalize request parameters:
 * http://oauth.googlecode.com/svn/spec/branches/1.0/drafts/7/spec.html#anchor14
 */
static int cmpstringp(const void *p1, const void *p2) {
   return strcmp(* (char * const *)p1, * (char * const *)p2);
}

/**
 * sign an oAuth request URL.
 *
 * if 'postargs' is NULL a "GET" request is signed and the 
 * signed URL is returned. Else this fn will modify 'postargs' 
 * to point to memory that contains the signed POST-variables 
 * and returns the base URL.
 *
 * both, the return value and (if given) 'postargs' need to be freed
 * by the caller.
 *
 * @param url The request URL to be signed. append all GET or POST 
 * query-parameters separated by either '?' or '&' to this parameter.
 *
 * @param postargs This parameter points to an area where the return value
 * is stored. If 'postargs' is NULL, no value is stored.
 *
 * @param method specify the signature method to use. It is of type 
 * \ref OAuthMethod and most likely \ref OA_HMAC.
 *
 * @param c_key consumer key
 * @param c_secret consumer secret
 * @param t_key token key
 * @param t_secret token secret
 *
 * @return the signed url or NULL if an error occurred.
 *
 */
char *oauth_sign_url (const char *url, char **postargs, 
  OAuthMethod method, 
  const char *c_key, //< consumer key - posted plain text
  const char *c_secret, //< consumer secret - used as 1st part of secret-key 
  const char *t_key, //< token key - posted plain text in URL
  const char *t_secret //< token secret - used as 2st part of secret-key
  ) {

  // split url arguments
  int  argc;
  char **argv = NULL;
  char *tmp;
  argc = split_url_parameters(url, &argv);

#define ADD_TO_ARGV \
  argv=(char**) xrealloc(argv,sizeof(char*)*(argc+1)); \
  argv[argc++]=xstrdup(oarg); 

  // add oAuth specific arguments
  char oarg[1024];
  snprintf(oarg, 1024, "oauth_nonce=%s", (tmp=gen_nonce()));
  ADD_TO_ARGV;
  free(tmp);

  snprintf(oarg, 1024, "oauth_timestamp=%li", time(NULL));
  ADD_TO_ARGV;
  if (t_key && strlen(t_key) >0) {
    tmp = url_escape(t_key); // FIXME: check if we need to escape this here
    snprintf(oarg, 1024, "oauth_token=%s", tmp);
    ADD_TO_ARGV;
    if(tmp) free(tmp);
  }

  tmp = url_escape(c_key); // FIXME: check if we need to escape this here
  snprintf(oarg, 1024, "oauth_consumer_key=%s", tmp);
  ADD_TO_ARGV;
  if(tmp) free(tmp);

  snprintf(oarg, 1024, "oauth_signature_method=%s",
      method==0?"HMAC_SHA1":method==1?"RSA-SHA1":"PLAINTEXT");
  ADD_TO_ARGV;

  snprintf(oarg, 1024, "oauth_version=1.0");
  ADD_TO_ARGV;

  // sort parameters
  qsort(&argv[1], argc-1, sizeof(char *), cmpstringp);

  // serialize URL
  char *query= serialize_url_parameters(argc, argv);

  // generate signature
  char *okey, *odat, *sign, *senc;
  okey = catenc(2, c_secret, t_secret);
  odat = catenc(3, postargs?"POST":"GET", argv[0], query);
#ifdef DEBUG_OAUTH
  printf ("\n\ndata to sign: %s\n\n", odat);
  printf ("key: %s\n\n", okey);
#endif
  switch(method) {
    case OA_RSA:
      sign = oauth_sign_rsa_sha1(odat,okey);
    	break;
    case OA_PLAINTEXT:
      sign = oauth_sign_plaintext(odat,okey);
    	break;
    default:
      sign = oauth_sign_hmac_sha1(odat,okey);
  }
  senc = url_escape(sign);
  free(odat); 
  free(okey);
  free(sign);

  // append signature to query args.
  snprintf(oarg, 1024, "oauth_signature=%s",senc);
  ADD_TO_ARGV;
  free(senc);

  // build URL params
  int i;
  char *result = (char*) xmalloc(sizeof(char)); 
  *result='\0';
  for(i=(postargs?1:0); i< argc; i++) {
    int len = 0;
    if(result) len+=strlen(result);
    len+=strlen(argv[i])+3;
    result= (char *)xrealloc(result,len*sizeof(char));
    strcat(result, ((i==0||(postargs&&i==1))?"":((!postargs&&i==1)?"?":"&")));
    strcat(result, argv[i]);
    free(argv[i]);
  }

  if(postargs) { 
    *postargs = result;
    result = xstrdup(argv[0]);
    free(argv[0]);
  }
  if(argv) free(argv);
  if(query) free(query);

  return result;
}
