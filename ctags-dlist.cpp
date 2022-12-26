#include <cstdio>
#include <cstdlib>
#include <cassert>

// Set a maximum expected path length.
// Throw an error if paths are longer than MAX.
#define MAX 10240

int main(int argc, char* argv[])
{
    // Guard against bad inputs
    if (argc != 2)                                       // Stop if wrong number of args
    {
        puts("Usage: ./ctags-dlist FILE\n"
             "EXAMPLE: ./ctags-dlist main.d"
            );
        return EXIT_FAILURE;
    }

    // Open dependencies file
    FILE *fi = fopen(argv[1], "r");
    if (  fi==NULL  )
    {
        perror("Cannot open dependencies file");
        return EXIT_FAILURE;
    }

    // Open output file
    FILE *fo = fopen("headers.txt", "w");
    if (  fo==NULL  )
    {
        perror("Cannot open output file headers.txt");
        fclose(fi);
        return EXIT_FAILURE;
    }

    /* *************Do the thing***************
     * Get each path in the dependencies file.
     * Store the path in a buffer.
     * Check whether to ignore the path.
     * If it's a good path, write it to headers.txt.
     * *******************************/

    char buf[MAX]; char *b = buf; int len=0;            // buf : buffer path strings
    int c; while(  (c=fgetc(fi)) != EOF  )              // Get next char from main.d
    {
        if(  (c==' ') || (c=='\n')  )                   // Whitespace : path sep
        {
            // Ignore value in buffer if it is not a header path
            if (
                 // Buffer is empty
                    (  len==0  )
                 // Buffer is '\'
                 || (  *buf == '\\'  )
                 // Buffer ends with "cpp"
                 || ( (len>3) && (*(b-3)=='c') && (*(b-2)=='p') && (*(b-1)=='p') )
                 // Buffer ends with "o:"
                 || ( (len>2) && (*(b-2)=='o') && (*(b-1)==':') )
               )
            { // Ignore value in buffer
               /* *************DEBUG***************
                printf("Ignore this: ");
                // Write value in buffer to output file
                for( int i=0; i<len; i++ )
                {
                    putchar(buf[i]);
                }
                putchar('\n');
                * *******************************/
            }
            else
            {
                // Write value in buffer to output file
                for( int i=0; i<len; i++ )
                {
                    fputc(buf[i], fo);
                }
                fputc('\n', fo);
            }
            // Prepare for new path
            b = buf; len=0;                             // Reset path buffer
        }
        else                                            // Store non-whitespace chars
        {
            *b = c; b++; len++;                         // Store char in buf
            assert(len<MAX);                            // Guard against buf overflow
        }
    }
    assert(len==0);                                     // Check I saved the last path!

    // Close dependencies file
    fclose(fi);                                         // Close dependencies file
    fclose(fo);                                         // Close output file
}

