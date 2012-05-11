// Copyright © 2011 Moritz Hanke

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "bit_macros.h"
#include "cip.h"

// decode Packet Primary Header
void * decode_pph(void * buffer, struct cip * cip);

// decode Secondary Header
void * decode_sh(void * buffer, struct cip * cip);

// decode Source Data Field
void * decode_sdf(void * buffer, struct cip * cip);

// decode Preprocessor Parameters
void * decode_preprocessor(void * buffer, struct cip * cip);

// decode Entopy Coder Parameters
void * decode_entropy_coder(void * buffer, struct cip * cip);

// decode Instrument Configuration
void * decode_instrument_configuration(void * buffer, struct cip * cip);

void * decode_cip(void * buffer, struct cip * cip) {

   void * pdf; // Packet Data Field
   void * sdf; // Source Data Field
   void * cds; // Coded data sets

   pdf = decode_pph(buffer, cip);

   if (cip->header.secondary_header_available)
      sdf = decode_sh(pdf, cip);
   else
      sdf = pdf;

   cds = decode_sdf(sdf, cip);

   if ((char*)cds > (char*)pdf + cip->header.data_length + 1) {

      fprintf(stderr, "Packet Data Field is bigger than specified in CIP Packet Primary Header\n");
      exit(EXIT_FAILURE);
   }

   return (char*)pdf + cip->header.data_length + 1;
}

// decode Primary Header
void * decode_pph(void * buffer, struct cip * cip) {

   char * cip_buf = buffer;

   // Packet Version Number

   cip->header.packet_version_number = (cip_buf[0] >> 5) & B3 ;

   if (cip->header.packet_version_number != 0) {

      fprintf(stderr, "Undefined Packet Version Number\n");
      exit(EXIT_FAILURE);
   }

   // Packet Identification
   {
      // Packet Type

      cip->header.type = ((cip_buf[0] >> 4) & B1)?TELECOMMAND:TELEMETRY;

      // is a Secondary Header available

      cip->header.secondary_header_available = (cip_buf[0] >> 3) & B1;

      // Application Process Identifer

      cip->header.apid = ((unsigned)(cip_buf[0] & B3) << 8) | cip_buf[1];
   }

   // Packet Sequence Control
   {
      // Sequence Flags

      cip->header.sequence_flag = (cip_buf[2] >> 6) & B2;

      // Packet Sequence Count or Packet Name

      assert(sizeof(unsigned) > 2);

      cip->header.sequence_count = ((unsigned)(cip_buf[2] & B6) << 8) |
                                   cip_buf[3];
   }

   // Packet Data Length

   cip->header.data_length = ((unsigned)cip_buf[4] << 8) | cip_buf[5];

   return (char*)cip + 6; // the Packet Primary Header has a size of 48
                          // bits 
}

void * decode_sh(void * buffer, struct cip * cip) {

   // The Secondary Header contains information like time and position
   // or attitude of a spacecraft.
   // The handling of a Secondary Header is currently not implemented.

   fprintf(stderr, "Compression Identification Packet contains a Secondary Header\n");
   exit(EXIT_FAILURE);

   return buffer;
}

void * decode_sdf(void * buffer, struct cip * cip) {

   char * sdf = buffer;

   // Grouping Data Length (number of packets containing compressed data)

   cip->sdf.grouping_data_length = (((unsigned)(sdf[0] & B4) << 8) |
                                    sdf[1]) + 1;

   // check whether a compression is used for the current group

   if (sdf[2] > 1) {

      fprintf(stderr, "Undefined compression technique\n");
      exit(EXIT_FAILURE);
   }

   cip->sdf.compression_technique = sdf[2];

   // Reference Sample Interval

   cip->sdf.reference_sample_interval = sdf[3]+1;


   if (cip->sdf.compression_technique == LOSSLESS) {

      // decode Preprocessor Parameters
      sdf = decode_preprocessor(sdf, cip);
   }

   // decode Entropy Coder Parameters
   sdf = decode_entropy_coder(sdf, cip);

   // decode Instrument Configuration
   sdf = decode_instrument_configuration(sdf, cip);

   return sdf;
}

void * decode_preprocessor(void * buffer, struct cip * cip) {

   char * sdf = buffer;

   unsigned header;

   // check for correct header
   header = (sdf[0] >> 6) & B2;
   if (header != 0) {

      fprintf(stderr, "Wrong header for preprocessor field\n");
      exit(EXIT_FAILURE);
   }

   // get preprocessor presents
   cip->sdf.preprocessor_config.preprocessor_present = (sdf[0] >> 5) & B1;

   // if a preprocessor is present
   if (cip->sdf.preprocessor_config.preprocessor_present) {

      // Predictor type
      switch ((sdf[0] >> 2) & B3) {

         case (0):
            cip->sdf.preprocessor_config.predictor_type = BYPASS_P;
            break;
         case (1):
            cip->sdf.preprocessor_config.predictor_type = UNIT_DELAY_P;
            break;
         case (7):
            cip->sdf.preprocessor_config.predictor_type = APP_SPECIFIC_P;
            break;
         default:
            fprintf(stderr, "Invalid predictor type\n");
            exit(EXIT_FAILURE);
      };

      // Mapper type
      switch (sdf[0] & B2) {

         case (0):
            cip->sdf.preprocessor_config.mapper_type = STANDARD_M;
            break;
         case (3):
            cip->sdf.preprocessor_config.mapper_type = APP_SPECIFIC_M;
            break;
         default:
            fprintf(stderr, "Invalid mapper type\n");
            exit(EXIT_FAILURE);
      };
   }

   // Block size (J)
   switch ((sdf[1] >> 6) & B2) {

      case (0):
         cip->sdf.preprocessor_config.block_size = 8;
         break;
      case (1):
         cip->sdf.preprocessor_config.block_size = 16;
         break;
      case(3):
         fprintf(stderr, "Missing implementation for application-specific block size\n");
         exit(EXIT_FAILURE);
      default:
         fprintf(stderr, "Invalid block size\n");
         exit(EXIT_FAILURE);
   };

   // Data sense
   cip->sdf.preprocessor_config.data_sense = (sdf[1] >> 5) & 1;

   // check whether preprocessor presents and data sense match
   if (!cip->sdf.preprocessor_config.preprocessor_present &&
       cip->sdf.preprocessor_config.data_sense != POSITIVE) {

      fprintf(stderr, "If no preprocessor is present, the data sense has to be positive.\n");
      exit(EXIT_FAILURE);
   }

   // Input data sample resolution
   cip->sdf.preprocessor_config.resolution = (sdf[1] & B5) + 1;

   return (char*)buffer + 2; // the preprocessor field has a size of 2 byte
}

void * decode_entropy_coder(void * buffer, struct cip * cip) {

   char * sdf = buffer;

   unsigned header;

   // check for correct header
   header = (sdf[0] >> 6) & B2;
   if (header != 1) {

      fprintf(stderr, "Wrong header for entropy coder field\n");
      exit(EXIT_FAILURE);
   }

   // Entropy Coder option
   switch ((sdf[0] >> 4) & B2) {

      case (1):
         if (cip->sdf.preprocessor_config.resolution > 8) {

            fprintf(stderr, "Entropy coder option does not match input data sample resolution\n");
            exit(EXIT_FAILURE);
         }
         cip->sdf.entropy_coder_config.option = OPTION_S;
         break;
      case (2):
         if ((cip->sdf.preprocessor_config.resolution <= 8) ||
             (cip->sdf.preprocessor_config.resolution > 16)) {

            fprintf(stderr, "Entropy coder option does not match input data sample resolution\n");
            exit(EXIT_FAILURE);
         }
         cip->sdf.entropy_coder_config.option = OPTION_M;
         break;
      case (3):
         if ((cip->sdf.preprocessor_config.resolution <= 16) ||
             (cip->sdf.preprocessor_config.resolution > 32)) {

            fprintf(stderr, "Entropy coder option does not match input data sample resolution\n");
            exit(EXIT_FAILURE);
         }
         cip->sdf.entropy_coder_config.option = OPTION_L;
         break;
      default:
         fprintf(stderr, "Invalid entropy coder option\n");
         exit(EXIT_FAILURE);
   };

   assert(sizeof(unsigned) > 2);

   // Number of coded data sets per packet
   cip->sdf.entropy_coder_config.num_cds_per_packet =
      (((unsigned)(sdf[0] & B4) << 8) | sdf[1]) + 1;

   return (char*)buffer + 2; // entropy coder field has a size of 2 byte
}

void * decode_instrument_configuration(void * buffer, struct cip * cip) {

   // Instrument Configuration is mission specific.
   // This implementation assumes that there is no.
   // In an one is added later, the first 2 bits of buffer have to 10.

   return buffer;
}
