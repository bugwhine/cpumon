#include <gtk/gtk.h>
#include <sensors/sensors.h>
#include <sensors/error.h>
#include <regex.h>        
#include <assert.h>        
#include <stdlib.h>

#define MAX_CPU (4)

const sensors_chip_name *name;
const sensors_subfeature *sub;

static void init_get_temp()
{
    sensors_chip_name match;
    const sensors_feature *feature;
    int a, b;
    int chip_nr = 0;

    sensors_init(NULL);

    sensors_parse_chip_name("k10temp-pci-00c3", &match);
    name = sensors_get_detected_chips(&match, &chip_nr);

    a = 0;
    while ((feature = sensors_get_features(name, &a))) {
        b = 0;
        while ((sub = sensors_get_all_subfeatures(name, feature, &b))) {
            if ((sub->flags & SENSORS_MODE_R) && !strcmp(sub->name, "temp1_input"))
                break;
        }
    }
}

static int get_temp()
{
    double val;
    sensors_get_value(name, sub->number, &val);
    return (int) val;
}

static void done_get_temp()
{
    sensors_cleanup();
}

void get_cpu_usage(int usage[])
{
    static unsigned long last[MAX_CPU][4];
    unsigned long current[MAX_CPU][4];
    char buf[256];
    FILE *fp;
    int cpu = 0;

    fp = fopen("/proc/stat","r");
    /* read and forget first line (system load) */
    fgets(buf, sizeof(buf), fp);
    while (cpu < MAX_CPU)
    {
        fgets(buf, sizeof(buf), fp);
        sscanf(buf,"%*s %lu %lu %lu %lu",&current[cpu][0],&current[cpu][1],&current[cpu][2],&current[cpu][3]);

        usage[cpu] = 100 * ((current[cpu][0]+current[cpu][1]+current[cpu][2]) - 
            (last[cpu][0]+last[cpu][1]+last[cpu][2])) / 
            ((current[cpu][0]+current[cpu][1]+current[cpu][2]+current[cpu][3]) - 
                (last[cpu][0]+last[cpu][1]+last[cpu][2]+last[cpu][3]));
        cpu++;
    }

    memcpy(last, current, sizeof(last));

    fclose(fp);
}

void get_cpu_freq(int freq[])
{
    FILE *fp;
    int cpu;

    regex_t regex1, regex2;
    char buf[128];

    assert(regcomp(&regex1, "^processor\\s*:\\s(\\d*)", REG_EXTENDED) == 0);
    assert(regcomp(&regex2, "^cpu MHz\\s*:\\s(\\d*)", REG_EXTENDED) == 0);

    fp = fopen("/proc/cpuinfo","r");
    while (fgets(buf, sizeof(buf),fp)) 
    {
        regmatch_t match[2];
        if (regexec(&regex1, buf, 2, match, 0) == 0) 
        {
            cpu = atoi(&buf[match[1].rm_so]);
            continue;
        }

        if (regexec(&regex2, buf, 2, match, 0) == 0) 
        {
            freq[cpu] = atoi(&buf[match[1].rm_so]);
        }
    }

    fclose(fp);
}

int get_min_freq()
{
    return 1700;
}

int get_max_freq()
{
    return 4100;
}

static int usage[MAX_CPU];
static int freq[MAX_CPU];

static gboolean update(gpointer data)
{
    GtkGrid     *grid = GTK_GRID(data);
    GtkWidget   *levelbar;
    GtkWidget   *valuelabel;
    int value, cpu;
    char valuestring[20];

    value = get_temp();
    levelbar = GTK_WIDGET(gtk_grid_get_child_at(grid, 1, 0));
    gtk_level_bar_set_value(GTK_LEVEL_BAR(levelbar), value);

    valuelabel = GTK_WIDGET(gtk_grid_get_child_at(grid, 2, 0));
    snprintf(valuestring, sizeof(valuestring), "%d \u2103", value);
    gtk_label_set_text(GTK_LABEL(valuelabel), valuestring);

    get_cpu_usage(usage);
    get_cpu_freq(freq);

    for (cpu = 0; cpu < MAX_CPU; cpu++) 
    {
        levelbar = GTK_WIDGET(gtk_grid_get_child_at(grid, 1, 1+cpu*2));
        gtk_level_bar_set_value(GTK_LEVEL_BAR(levelbar), usage[cpu]);

        valuelabel = GTK_WIDGET(gtk_grid_get_child_at(grid, 2, 1+cpu*2));
        snprintf(valuestring, sizeof(valuestring), "%d %%", usage[cpu]);
        gtk_label_set_text(GTK_LABEL(valuelabel), valuestring);

        levelbar = GTK_WIDGET(gtk_grid_get_child_at(grid, 1, 2+cpu*2));
        gtk_level_bar_set_value(GTK_LEVEL_BAR(levelbar), freq[cpu]);

        valuelabel = GTK_WIDGET(gtk_grid_get_child_at(grid, 2, 2+cpu*2));
        snprintf(valuestring, sizeof(valuestring), "%d MHz", freq[cpu]);
        gtk_label_set_text(GTK_LABEL(valuelabel), valuestring);
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    GtkBuilder      *builder; 
    GtkWidget       *window;
    GtkWidget       *grid;
    GtkWidget       *widget;
 
    init_get_temp();
    gtk_init();
 
    builder = gtk_builder_new();
    gtk_builder_add_from_file (builder, "glade/cpumon.glade", NULL);
 
    window = GTK_WIDGET(gtk_builder_get_object(builder, "cpumon"));
    gtk_builder_connect_signals(builder, NULL);

    grid = GTK_WIDGET(gtk_builder_get_object(builder, "cpumonGrid"));

    for (int i = 0; i < MAX_CPU; i++) 
    {
        char buf[20];

        snprintf(buf, sizeof(buf), "CPU%d", i);
        widget = gtk_label_new(buf);
        gtk_grid_attach(GTK_GRID(grid), widget, 0, 1+(2*i), 1, 1);

        widget = gtk_level_bar_new_for_interval(0, 100);
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 1+(2*i), 1, 1);

        widget = gtk_label_new("%");
        gtk_grid_attach(GTK_GRID(grid), widget, 2, 1+(2*i), 1, 1);

        widget = gtk_level_bar_new_for_interval(get_min_freq(), get_max_freq());
        gtk_grid_attach(GTK_GRID(grid), widget, 1, 2+(2*i), 1, 1);

        widget = gtk_label_new("MHz");
        gtk_grid_attach(GTK_GRID(grid), widget, 2, 2+(2*i), 1, 1);
    }

    gtk_widget_show(window);

    gdk_threads_add_timeout(1000, update, grid);             
    gtk_main();
 
    g_object_unref(builder);
    done_get_temp();

    return 0;
}
 
// called when window is closed
void on_cpumon_destroy()
{
    gtk_main_quit();
}