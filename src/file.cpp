/*
 * ============================================================================
 *  MaqOS — Console File Manager (CLI)
 * ============================================================================
 *  File:    file.cpp
 *  Purpose: A command-line file management utility that provides basic
 *           file operations: create, inspect, delete, and list files.
 *           Runs in a terminal/console (not a GUI application).
 *
 *  Operations:
 *      1. Create File  — Creates a new file with a default message inside
 *      2. Show Info    — Displays file size, permissions (octal), and last modified date
 *      3. Delete File  — Removes a file from disk using unlink()
 *      4. Show Files   — Lists all files in the current directory (via `ls`)
 *
 *  The program loops until the user chooses not to continue.
 *
 *  Dependencies: POSIX (sys/stat.h, unistd.h) for file operations
 *  Launched by:  localDisk.cpp when "Local Disk (C:)" is clicked
 * ============================================================================
 */
#include <iostream>     // cin/cout for user interaction
#include <fstream>      // ofstream for creating files
#include <sys/stat.h>   // stat() — retrieves file metadata (size, permissions, timestamps)
#include <unistd.h>     // unlink() — deletes a file from the filesystem
#include <cstring>      // String utilities
#include <ctime>        // ctime() — converts time_t to human-readable string

using namespace std;

/*
 * createFile() — Creates a new file and writes a default message to it.
 * @param filename  The full filename including extension (e.g., "notes.txt")
 */
void createFile(const string& filename) {
    ofstream file(filename);
    if (!file) {
        cout << "Error creating file.\n";
        return;
    }
    file << "This file was created by a C++ program.\n";
    file.close();
    cout << "File created and written to: " << filename << "\n";
}

/*
 * showFileInfo() — Displays metadata about a file using the POSIX stat() call.
 * Shows: file size in bytes, UNIX permissions (octal), and last modification time.
 * @param filename  The file to inspect
 */
void showFileInfo(const string& filename) {
    struct stat fileStat;
    if (stat(filename.c_str(), &fileStat) == -1) {
        cout << "Error getting file info!\n";
        return;
    }

    cout << "\nFile information for " << filename << ":\n";
    cout << "Size: " << fileStat.st_size << " bytes\n";
    cout << "Permissions: " << oct << (fileStat.st_mode & 0777) << dec << "\n";  // Octal format
    cout << "Last modified: " << ctime(&fileStat.st_mtime);  // Human-readable timestamp
}

/*
 * deleteFile() — Removes a file from the filesystem using POSIX unlink().
 * @param filename  The file to delete
 */
void deleteFile(const string& filename) {
    if (unlink(filename.c_str()) != 0) {
        cout << "Error deleting file!\n";
        return;
    }
    cout << "File deleted: " << filename << "\n";
}

/*
 * showFiles() — Lists all files in the current working directory.
 * Uses the system `ls` command (Linux/macOS).
 */
void showFiles() {
    system("ls");
}

/*
 * main() — Interactive console menu loop.
 * Prompts the user to choose an operation, asks for the filename,
 * executes the operation, and asks whether to continue.
 */
int main() {
    string cont;
    int choice;

    do {
        string name, type, filename;

        // Display the menu options
        cout << "\nChoose an option:\n";
        cout << "1. Create file\n";
        cout << "2. Show file info\n";
        cout << "3. Delete file\n";
        cout << "4. Show files\n";
        cout << "Enter your choice (1-4): ";
        cin >> choice;

        // For file operations (1-3), ask for filename and extension
        if (choice >= 1 && choice <= 3) {
            cout << "Enter the file name (without extension): ";
            cin >> name;
            cout << "Enter the file extension (e.g., txt): ";
            cin >> type;
            filename = name + "." + type;  // Combine into "name.ext"
        }

        // Execute the chosen operation
        switch (choice) {
            case 1: createFile(filename);   break;
            case 2: showFileInfo(filename); break;
            case 3: deleteFile(filename);   break;
            case 4: showFiles();            break;
            default: cout << "Invalid choice!\n";
        }

        // Ask if the user wants to continue
        cout << "\nDo you want to perform another operation? (y/n): ";
        cin >> cont;
    } while (cont != "n" && cont != "N");

    cout << "Exiting program.\n";
    return 0;
}
