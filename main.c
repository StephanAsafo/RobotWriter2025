#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rs232.h"
#include "serial.h"

#define MAX_COMMANDS 10000   // Max number of movement commands that can be stored
#define MAX_POINTS 20
#define FONT_HEIGHT_UNITS 18  // Font is defined with 18-unit height

typedef struct {      //Storing cooridnates and pen state
    int x, y;
    int pen; // 1 = down, 0 = up
} Command;

Command commands[MAX_COMMANDS];
int commandCount = 0;
int currentX = 0;
int currentY = 0;
float scale = 1.0f;

void penUp() {     //Lift the pen
    if (commandCount < MAX_COMMANDS) {
        commands[commandCount++] = (Command){currentX, currentY, 0};
    }
}

void penDown() {   //Lower the pen
    if (commandCount < MAX_COMMANDS) {
        commands[commandCount++] = (Command){currentX, currentY, 1};
    }
}

void moveTo(int x, int y) {    //Move to a new position without drawing
    currentX = x;
    currentY = y;
}

void lineTo(int x, int y) {    //Move and draw to a new position
    moveTo(x, y);
    penDown();
}

void drawChar(int c, int x_offset, int y_offset) {  // Draw a single character using stroke data
    FILE *file = fopen("SingleStrokeFont.txt", "r");
    if (!file) {
        printf("Error: Could not open font file.\n");
        return;
    }

    char line[256];
    int charIndex = -1;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "999", 3) == 0) {
            int index, count;
            sscanf(line, "999 %d %d", &index, &count);
            charIndex = index;
            if (charIndex == c) {
                for (int i = 0; i < count; i++) {
                    if (!fgets(line, sizeof(line), file)) break;
                    int dx, dy, pen;
                    sscanf(line, "%d %d %d", &dx, &dy, &pen);
                    int scaledX = x_offset + (int)(dx * scale);
                    int scaledY = y_offset + (int)(dy * scale);
                    moveTo(scaledX, scaledY);
                    if (pen) {
                        penDown();
                    } else {
                        penUp();
                    }
                }
                break;
            } else {
                for (int i = 0; i < count; i++) fgets(line, sizeof(line), file);
            }
        }
    }

    fclose(file);
}

void drawText(const char *text, int x, int y) {
    int spacing = (int)(scale * 20); // scale spacing with height
    for (int i = 0; text[i] != '\0'; i++) {
        drawChar((int)text[i], x + i * spacing, y);
    }
}

void SendCommands(char *buffer) {
    PrintBuffer(&buffer[0]);
    WaitForReply();
    Sleep(100);
}

void outputGCode() {
    char buffer[100];

    if (CanRS232PortBeOpened() == -1) {
        printf("❌ Unable to open the COM port (specified in serial.h)\n");
        return;
    }

    printf("✅ Waking up the robot...\n");
    sprintf(buffer, "\n");
    PrintBuffer(buffer);
    Sleep(100);
    WaitForDollar();

    // Initialization commands, sned drawing commands over serial using G-code
    sprintf(buffer, "G1 X0 Y0 F1000\n"); SendCommands(buffer);
    sprintf(buffer, "M3\n"); SendCommands(buffer);
    sprintf(buffer, "S0\n"); SendCommands(buffer);

    for (int i = 0; i < commandCount; i++) {
        if (commands[i].pen) {
            sprintf(buffer, "S1000\n"); SendCommands(buffer);
            sprintf(buffer, "G1 X%d Y%d\n", commands[i].x, commands[i].y);
            SendCommands(buffer);
        } else {
            sprintf(buffer, "S0\n"); SendCommands(buffer);
            sprintf(buffer, "G0 X%d Y%d\n", commands[i].x, commands[i].y);
            SendCommands(buffer);
        }
    }

    CloseRS232Port();
    printf("✅ COM port closed.\n");
}

int main() {
    int height_mm;
    printf("Enter desired text height in mm (4 to 10): ");
    scanf("%d", &height_mm);

    if (height_mm < 4 || height_mm > 10) {
        printf("Invalid height. Please enter a value between 4 and 10.\n");
        return 1;
    }
//Set scaling factor for font based on user input height
    scale = (float)height_mm / FONT_HEIGHT_UNITS;

    // Open test.txt and draw multiple lines
    FILE *textFile = fopen("test.txt", "r");
    if (!textFile) {
        printf("❌ Could not open test.txt\n");
        return 1;
    }

    char line[256];
    int lineNumber = 0;
    int lineSpacing = (int)(scale * 25);  // Vertical spacing between lines

    while (fgets(line, sizeof(line), textFile)) {
        // Strip newline
        line[strcspn(line, "\n")] = '\0';

        // Draw each line lower on Y-axis
        drawText(line, 0, -lineNumber * lineSpacing);
        lineNumber++;
    }

    fclose(textFile);

    outputGCode();
    return 0;
}
