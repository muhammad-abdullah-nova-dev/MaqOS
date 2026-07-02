#include <gtk/gtk.h>

// Function to close the window after delay
gboolean close_window_after_delay(gpointer data) {
    GtkWidget *window = GTK_WIDGET(data);
    gtk_window_close(GTK_WINDOW(window));
    return FALSE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Create window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Loading...");
    gtk_window_set_default_size(GTK_WINDOW(window), 1600, 1000);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 0);

// Create CSS provider and load CSS
GtkCssProvider *cssProvider = gtk_css_provider_new();
gtk_css_provider_load_from_data(cssProvider,
    "window, #mainbox { background-color: black; }"
    "#mainbox { margin: 0; padding: 0; }"
    "#mainbox image { margin: 0; padding: 0; }"
    ,
    -1, NULL);

// Create main container (box) to fill the entire window
GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
gtk_widget_set_name(mainBox, "mainbox");

// Configure the box to expand and fill the entire window
gtk_widget_set_hexpand(mainBox, TRUE);
gtk_widget_set_vexpand(mainBox, TRUE);
gtk_widget_set_halign(mainBox, GTK_ALIGN_FILL);
gtk_widget_set_valign(mainBox, GTK_ALIGN_FILL);
    // Apply CSS to container
    GtkStyleContext *context = gtk_widget_get_style_context(mainBox);
    gtk_style_context_add_provider(context,
        GTK_STYLE_PROVIDER(cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    // Load and add the GIF image
    GtkWidget *image = gtk_image_new_from_file("assets/images/loading.gif");
    gtk_box_pack_start(GTK_BOX(mainBox), image, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(window), mainBox);

    // Handle window close
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    // Auto-close after 16 seconds
    g_timeout_add_seconds(16, close_window_after_delay, window);

    gtk_main();
    return 0;
}

