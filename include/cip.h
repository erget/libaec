// Copyright © 2011 Moritz Hanke

// Based on Lossless Data Compression. Blue Book. Issue 1. May 1997.
// CCSDS 121.0-B-1
// 
// http://public.ccsds.org/publications/bluebooks.aspx
// http://public.ccsds.org/publications/archive/121x0b1c2.pdf

// Compression Identification Packet (optinal)
// The CIP is used to configure the decompressor in case no configuration
// data is available.
// It is comprised of the cip Primary Header (as described in "CCSDS Space
// Packet Protocol Blue Book") and the Packet Data Field.

enum cip_packet_type {

   TELEMETRY,   // reporting
   TELECOMMAND, // requesting (makes no sense for compression)
};

enum cip_sequence_flag {

   CONTINUE    = 0,
   FIRST       = 1,
   LAST        = 2,
   UNSEGMENTED = 3,
};

enum cip_compression_technique {

   NO       = 0, // no compression is used
   LOSSLESS = 1, // lossless compression is used
};

enum cip_predictor_type {

   BYPASS_P       = 0, // bypass predictor
   UNIT_DELAY_P   = 1, // unit delay predictor
   APP_SPECIFIC_P = 7, // application-specific predictor
};

enum cip_mapper_type {

   STANDARD_M     = 0, // standard prediction error mapper as described in the reference
   APP_SPECIFIC_M = 3, // application-specific mapper
};

enum cip_data_sense {

   TWOS_COMPLEMENT = 0, // two's complement
   POSITIVE        = 1, // positive (mandatory if preprocessor is bypassed)
};

enum entropy_coder_option {

   OPTION_S = 1, // for resolution      n <= 8
   OPTION_M = 2, // for resolution  8 < n <= 16
   OPTION_L = 3, // for resolution 16 < n <= 32
};

struct cip_header {

   //----------------------------
   // Packet Primary Header data
   //----------------------------

   char packet_version_number; // Packet Version Number

   // Packet Identification
   enum cip_packet_type type; // Packet Type
   char secondary_header_available; // Sequence Header Flag
   unsigned apid; // Application Process Identifier

   // Packet Sequence Control
   enum cip_sequence_flag sequence_flag; // Sequence Flags
   unsigned sequence_count; // Packet Sequence Count
                            // in case of a TELECOMMAND type cip
                            // data package this contains the
                            // Packet Name

   unsigned data_length; // length of the Packet Data Field - 1 in byte
};

struct cip_preprocessor_configuration {

   char preprocessor_present; // Preprocessor Status (0 - absent, 1 - present)

   enum cip_predictor_type predictor_type; // Predictor type (undefined if preprocessor is absent)

   enum cip_mapper_type mapper_type; // Mapper type (undefined if preprocessor is absent)

   char block_size; // Block size (8, 16 or application specific)

   enum cip_data_sense data_sense;

   char resolution; // Input data sample resolution (1-32)
};

struct cip_entropy_coder_configuration {

   enum entropy_coder_option option; // Entropy Coder option

   unsigned num_cds_per_packet; // Number of coded data sets per packet
};
   
struct cip_source_data_field {

   unsigned grouping_data_length; // Grouping Data Length

   enum cip_compression_technique compression_technique; // Compression Technique Identification

   unsigned reference_sample_interval; // Reference Sample Interval
                                       //  - in case a preprocessor is
                                       //    used that requires a
                                       //    reference sample
                                       //  - in case of the zero-block
                                       //    option it contains the
                                       //    interval of input data sample
                                       //    blocks

   struct cip_preprocessor_configuration preprocessor_config; // Preprocessor Configuration

   struct cip_entropy_coder_configuration entropy_coder_config; // Entropy Coder Configuration
};

struct cip {

   struct cip_header header; // Information from the Primary Header
   struct cip_source_data_field sdf; // Information on the data
};

void * decode_cip(void * buffer, struct cip * cip);
      // data: buffer starting with a cip
      // cip:  in case cip is not NULL the configuration information
      //       contained in the cip is written to it
      // returns a pointer to the compressed data without the cip
      // returns NULL in case an error occured while decoding
