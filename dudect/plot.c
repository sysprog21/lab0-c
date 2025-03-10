#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixture.h"

#define GNUPLOT_SCRIPT "gnuplot_script.gnu"
#define DATA_FILE "data.txt"

int n_tries = 0;

struct {
    double y[ENOUGH_MEASURE];
    int n_measures;
} data_buffer[TEST_TRIES];

static double max_data_value()
{
    double mx = 0;
    for (int i = 0; i < TEST_TRIES; i++) {
        double value = data_buffer[i].y[data_buffer[i].n_measures - 1];
        if (value > mx)
            mx = value;
    }
    return mx;
}

void init_data_buffer()
{
    n_tries = 0;
    for (int i = 0; i < TEST_TRIES; i++) {
        data_buffer[i].n_measures = 0;
    }
}

void next_try()
{
    n_tries++;
}

void add_data(double y)
{
    if (n_tries >= TEST_TRIES) {
        fprintf(stderr, "Buffer overflow\n");
        exit(1);
    }
    data_buffer[n_tries].y[data_buffer[n_tries].n_measures++] = y;
}

void save_data()
{
    FILE *fp = fopen(DATA_FILE, "w");
    if (!fp) {
        perror("Error opening file");
        exit(1);
    }
    fprintf(fp, "# X ");
    for (int i = 0; i < TEST_TRIES; i++) {
        fprintf(fp, "t%d ", i);
    }
    fprintf(fp, "\n");
    for (int i = 0; i < data_buffer[0].n_measures; i++) {
        fprintf(fp, "%d ", (int) i);
        for (int j = 0; j < TEST_TRIES; j++) {
            fprintf(fp, "%lf ", data_buffer[j].y[i]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

void generate_gnuplot_script(double threshold,
                             const char *title,
                             const char *output_file)
{
    FILE *gp = fopen(GNUPLOT_SCRIPT, "w");
    if (!gp) {
        perror("Error opening Gnuplot script file");
        exit(1);
    }

    int mx_val = (int) max_data_value();
    int y_range = mx_val < 25 ? 50 : mx_val * 2;


    fprintf(gp,
            "set terminal pngcairo enhanced size 800,400\n"
            "set output '%s'\n"
            "set title '%s'\n"
            "set xlabel '# measurements'\n"
            "set ylabel '|t| statistic'\n"
            "set grid front\n"
            "set xrange [0:%d]\n"
            "set yrange [0:%d]\n"

            // #Define the threshold as a function
            "f(x) = %lf\n"

            // #Fill the lower part(green) and upper part(red)
            "set style fill solid 0.3\n"
            "plot '%s' using 1:(f($1)) with filledcurves y1=0 lc rgb 'green' "
            "notitle, \\\n"
            "     '%s' using 1:(f($1)) with filledcurves y1=%d lc rgb 'red' "
            "notitle, \\\n"
            "     for [i=2:%d] '%s' using 1:i:(i) with lines lw 2 lc variable "
            "title sprintf('Test %%d', i-1)\n",
            output_file, title, data_buffer[0].n_measures, y_range, threshold,
            DATA_FILE, DATA_FILE, y_range, n_tries + 1, DATA_FILE);

    fclose(gp);
}

void plot_graph(double threshold, const char *title)
{
    save_data();
    char *output_file = malloc(strlen(title) + 5);
    if (!output_file) {
        perror("Error allocating memory");
        exit(1);
    }
    strncpy(output_file, title, strlen(title) + 1);
    strcat(output_file, ".png");
    generate_gnuplot_script(threshold, title, output_file);
    int ret = system("gnuplot " GNUPLOT_SCRIPT);
    if (ret != 0) {
        fprintf(stderr, "Error generating graph\n");
        exit(1);
    }
    printf("Graph generated: %s\n", output_file);
    free(output_file);
}