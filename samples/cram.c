/*  cram.c --  showcases the htslib api usage

    Copyright (C) 2023 Genome Research Ltd.

    Author: Vasudeva Sarma <vasudeva.sarma@sanger.ac.uk>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE

*/

/* The pupose of this code is to demonstrate the library apis and need proper error handling and optimization */

#include <getopt.h>
#include <unistd.h>
#include <htslib/sam.h>

/// print_usage - print the usage
/** @param fp pointer to the file / terminal to which usage to be dumped
returns nothing
*/
static void print_usage(FILE *fp)
{
    fprintf(fp, "Usage: cram infile reffile outdir\n\
Dumps the input file alignments in cram format in given directory\n\
1.cram has external reference\n\
2.cram has reference embedded\n\
3.cram has autogenerated reference\n\
4.cram has no reference data in it\n");
    return;
}

/// main_demo - start of the demo
/** @param argc - count of arguments
 *  @param argv - pointer to array of arguments
returns 1 on failure 0 on success
*/
int main(int argc, char *argv[])
{
    const char *inname = NULL, *outdir = NULL, *reffile = NULL;
    char *file1 = NULL, *file2 = NULL, *file3 = NULL, *file4 = NULL, *reffmt1 = NULL, *reffmt2 = NULL;
    int c = 0, ret = EXIT_FAILURE, size1 = 0, size2 = 0, size3 = 0;
    samFile *infile = NULL, *outfile1 = NULL, *outfile2 = NULL, *outfile3 = NULL, *outfile4 = NULL;
    sam_hdr_t *in_samhdr = NULL;
    bam1_t *bamdata = NULL;
    htsFormat fmt1 = {0}, fmt2 = {0}, fmt3 = {0}, fmt4 = {0};

    //cram infile reffile outdir
    if (argc != 4) {
        print_usage(stdout);
        goto end;
    }
    inname = argv[1];
    reffile = argv[2];
    outdir = argv[3];

    //allocate space for option string and output file names
    size1 = sizeof(char) * (strlen(reffile) + sizeof("cram,reference=") + 1);
    size2 = sizeof(char) * (strlen(reffile) + sizeof("cram,embed_ref=1,reference=") + 1);
    size3 = sizeof(char) * (strlen(outdir) + sizeof("/1.cram") + 1);

    reffmt1 = malloc(size1); reffmt2 = malloc(size2);
    file1 = malloc(size3); file2 = malloc(size3);
    file3 = malloc(size3); file4 = malloc(size3);

    if (!file1 || !file2 || !file3 || !file4 || !reffmt1 || !reffmt2) {
        printf("Failed to create buffers\n");
        goto end;
    }

    snprintf(reffmt1, size1, "cram,reference=%s", reffile);
    snprintf(reffmt2, size2, "cram,embed_ref=1,reference=%s", reffile);
    snprintf(file1, size3, "%s/1.cram", outdir); snprintf(file2, size3, "%s/2.cram", outdir);
    snprintf(file3, size3, "%s/3.cram", outdir); snprintf(file4, size3, "%s/4.cram", outdir);

    if (hts_parse_format(&fmt1, reffmt1) == -1 ||                       //using external reference - uses the M5/UR tags to get reference data during read
            hts_parse_format(&fmt2, reffmt2) == -1 ||                   //embed the reference internally
            hts_parse_format(&fmt3, "cram,embed_ref=2") == -1 ||        //embed autogenerated reference
            hts_parse_format(&fmt4, "cram,no_ref=1") == -1) {           //no reference data encoding at all
        printf("Failed to set output option\n");
        goto end;
    }

    //bam data storage
    if (!(bamdata = bam_init1())) {
        printf("Failed to initialize bamdata\n");
        goto end;
    }
    //open input file - r reading
    if (!(infile = sam_open(inname, "r"))) {
        printf("Could not open %s\n", inname);
        goto end;
    }
    //open output files - w write as SAM, wb write as BAM, wc as CRAM (equivalent to fmt3)
    outfile1 = sam_open_format(file1, "wc", &fmt1); outfile2 = sam_open_format(file2, "wc", &fmt2);
    outfile3 = sam_open_format(file3, "wc", &fmt3); outfile4 = sam_open_format(file4, "wc", &fmt4);
    if (!outfile1 || !outfile2 || !outfile3 || !outfile4) {
        printf("Could not open output file\n");
        goto end;
    }

    //read header, required to resolve the target names to proper ids
    if (!(in_samhdr = sam_hdr_read(infile))) {
        printf("Failed to read header from file!\n");
        goto end;
    }
    //write header
    if ((sam_hdr_write(outfile1, in_samhdr) == -1) || (sam_hdr_write(outfile2, in_samhdr) == -1) ||
            (sam_hdr_write(outfile3, in_samhdr) == -1) || (sam_hdr_write(outfile4, in_samhdr) == -1)) {
        printf("Failed to write header\n");
        goto end;
    }

    //check flags and write
    while ((c = sam_read1(infile, in_samhdr, bamdata)) >= 0) {
        if (sam_write1(outfile1, in_samhdr, bamdata) < 0 ||
                sam_write1(outfile2, in_samhdr, bamdata) < 0 ||
                sam_write1(outfile3, in_samhdr, bamdata) < 0 ||
                sam_write1(outfile4, in_samhdr, bamdata) < 0) {
            printf("Failed to write output data\n");
            goto end;
        }
    }
    if (-1 == c) {
        //EOF
        ret = EXIT_SUCCESS;
    }
    else {
        printf("Error in reading data\n");
    }
end:
#define IF_OL(X,Y) if((X)) {(Y);}           //if one liner
    //cleanup
    IF_OL(in_samhdr, sam_hdr_destroy(in_samhdr));
    IF_OL(infile, sam_close(infile));
    IF_OL(outfile1, sam_close(outfile1));
    IF_OL(outfile2, sam_close(outfile2));
    IF_OL(outfile3, sam_close(outfile3));
    IF_OL(outfile4, sam_close(outfile4));
    IF_OL(file1, free(file1));
    IF_OL(file2, free(file2));
    IF_OL(file3, free(file3));
    IF_OL(file4, free(file4));
    IF_OL(reffmt1, free(reffmt1));
    IF_OL(reffmt2, free(reffmt2));
    IF_OL(fmt1.specific, hts_opt_free(fmt1.specific));
    IF_OL(fmt2.specific, hts_opt_free(fmt2.specific));
    IF_OL(fmt3.specific, hts_opt_free(fmt3.specific));
    IF_OL(fmt4.specific, hts_opt_free(fmt4.specific));
    IF_OL(bamdata, bam_destroy1(bamdata));

    return ret;
}
