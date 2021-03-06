/**
 * @file main.c
 * @author My Self
 * @date 9 Sep 2012
 * @brief File containing example of doxygen usage for quick reference.
 *
 * Here typically goes a more extensive explanation of what the header
 * defines. Doxygens tags are words preceeded by either a backslash @\
 * or by an at symbol @@.
 * @see http://www.stack.nl/~dimitri/doxygen/docblocks.html
 * @see http://www.stack.nl/~dimitri/doxygen/commands.html
 */

#include <stdio.h>
#define LAST 10

/*!
 * \brief This is a sum function.
 *        sums from first to last.
 *
 * We used a for loop to implement this function.
 *
 * \param first the first integer to sum
 * \param last the last integer to sum
 * \return the result of sum
 */
int sum(int first, int last){
  int i, sum = 0;
  
  for ( i = first; i <= last; i++ ) {
    sum += i;
  }

  return sum;
}

int main()
{

  int sum = sum(1, LAST);
  printf("sum = %d\n", sum);

  return 0;
}

/**
 * Guess the file format.
 *
 * @param is_opened Whether the file is already opened; determines whether
 *                  demuxers with or without AVFMT_NOFILE are probed.
 * @param score_ret The score of the best detection.
 */
AVInputFormat *av_probe_input_format3(AVProbeData *pd, int is_opened, int *score_ret){
}

/**
 * Close an opened input AVFormatContext. Free it and all its contents
 * and set *s to NULL.
 */
void avformat_close_input(AVFormatContext **s){
}
