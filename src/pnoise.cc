#include <getopt.h>
#include <inttypes.h>
#include <iostream>
#include <new>
#include <libnoise/noise.h>

#define ROUND(x) ((x)>=0?(int64_t)((x)+0.5):(int64_t)((x)-0.5))
#define CLIPL(x, l) ((x) < (l)?(l): (x))
#define CLIPH(x, h) ((x) > (h)?(h): (x))
#define CLIP(x, l, h) CLIPL(CLIPH(x, h), l)
#define MIN(a, b) (((a) < (b))? (a): (b))

#define CHUNK_SIZE 1024

using namespace noise;
using namespace std;

int main (int argc, char** argv)
{
    int i, j, c, cn, chunk;
    int sflag = 0;
    int msb = 0;
    int length = 1024;
    int bits = 8;
    int bytes = 1;
    double value, scaling, spacing;
    char *buf;
    int64_t ivalue, min_x, max_x;

    while ((c = getopt (argc, argv, "hsmB:b:n:")) != -1)
        switch (c)
        {
        case 'B':
            bits = atoi(optarg);
            break;
        case 'b':
            bytes = atoi(optarg);
            break;
        case 'n':
            length = atoi(optarg);
            break;
        case 's':
            sflag = 1;
            break;
        case 'm':
            msb = 1;
            break;
        case '?':
            if (optopt == 'b' || optopt == 'B' || optopt == 'n')
                cerr << "Option -" << (char)optopt
                     << " requires an argument." << endl;
            else if (isprint (optopt))
                cerr << "Unknown option `-"
                     << (char)optopt << "'."
                     << endl;
            else
                cerr << "Unknown option character `\\x"
                     << hex << optopt
                     << "'." << endl;
        case 'h':
            cerr << "Usage: " << argv[0] << " [OPTION]" << endl;
            cerr << "-B Bit length of samples" << endl;
            cerr << "-b Byte length of samples, may be longer than bits"
                 << endl;
            cerr << "-n Number of samples" << endl;
            cerr << "-s Samples are signed" << endl;
            cerr << "-m Store MSB first, default is LSB first" << endl;
            return 1;
        default:
            abort ();
        }

    if ((bits - 1) / 8 >= bytes)
    {
        cerr << "ERROR: "
             << bits << " bits don't fit in "
             << bytes << " bytes\n" << endl;
        return 1;
    }

    buf = new char[CHUNK_SIZE * bytes];

    module::Perlin myModule;
    myModule.SetOctaveCount(1);

    if (sflag)
    {
        min_x = -(1ULL << (bits - 1));
        max_x = (1ULL << (bits - 1)) - 1;
    }
    else
    {
        min_x = 0;
        max_x = (1ULL << bits) - 1;
    }

    scaling = (double)((1ULL << (bits + 1)) - 1);
    spacing = 10000.0 / (double)(1ULL << bits);

    for (cn = 0; cn <= length / CHUNK_SIZE; cn++)
    {
        chunk = MIN(CHUNK_SIZE, length - cn * CHUNK_SIZE);

        for (i = 0; i < chunk; i++)
        {
            value = myModule.GetValue (spacing * (double)i, 0, 0) / 2.0;
            if (sflag)
            {
                ivalue = ROUND(value * scaling);
                ivalue = CLIP(ivalue, min_x, max_x);
            }
            else
            {
                ivalue = ROUND((value + 0.5) * scaling);
                ivalue = CLIP(ivalue, 0, max_x);
            }

            ivalue &= (1ULL << bits) - 1;
            if (msb)
            {
                for (j = 0; j < bytes; j++)
                    buf[i * bytes + j] = ivalue >> ((bytes - j - 1) * 8);
            }
            else
            {
                for (j = 0; j < bytes; j++)
                    buf[i * bytes + j] = ivalue >> (j * 8);
            }
        }
        cout.write(buf, chunk * bytes);
    }
    delete buf;
    return 0;
}
