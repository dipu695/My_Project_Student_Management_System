// main.cpp
// 2D Student Management GUI using OpenGL 2.1 (compatibility mode)
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iomanip>
using namespace std;

// ------------------------- stb_easy_font -------------------------
#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h" // place stb_easy_font.h in the same folder

// ------------------------- Student Structures -------------------------
class Student
{
public:
    string name;
    int roll;
    string grade;
    string department;
    float cgpa;
};
class Button
{
public:
    float x, y, w, h;
    string label;
    bool pressed = false;
    double pressTime = 0.0;
};
class InputBox
{
public:
    float x, y, w, h;
    string text;
    bool focused = false;
};

// Message popup structure
class MessagePopup
{
public:
    string text;
    double showTime;
    double duration;
    bool visible;

    MessagePopup() : text(""), showTime(0), duration(1.2), visible(false) {}

    void show(const string &msg, double currentTime)
    {
        text = msg;
        showTime = currentTime;
        visible = true;
    }

    bool isVisible(double currentTime) const
    {
        if (!visible)
            return false;
        if (currentTime - showTime > duration)
        {
            return false;
        }
        return true;
    }

    float getAlpha(double currentTime) const
    {
        if (!visible)
            return 0.0f;
        double elapsed = currentTime - showTime;
        if (elapsed > duration)
            return 0.0f;

        // Fade out in the last 0.5 seconds
        if (elapsed > duration - 0.5)
        {
            return (float)((duration - elapsed) / 0.5);
        }
        return 1.0f;
    }
};

// Details Panel structure
class DetailsPanel
{
public:
    bool visible;
    Student *currentStudent;
    double animationStart;
    double animationDuration;

    DetailsPanel() : visible(false), currentStudent(nullptr), animationStart(0), animationDuration(0.3) {}

    void show(Student *student, double currentTime)
    {
        currentStudent = student;
        if (!visible)
        {
            visible = true;
            animationStart = currentTime;
        }
    }

    void hide()
    {
        visible = false;
        currentStudent = nullptr;
    }

    float getSlideProgress(double currentTime) const
    {
        if (!visible)
            return 0.0f;

        double elapsed = currentTime - animationStart;
        if (elapsed >= animationDuration)
            return 1.0f;

        // Smooth easing
        float t = (float)(elapsed / animationDuration);
        return t * t * (3.0f - 2.0f * t); // Smoothstep
    }
};

// Sorting state
enum class SortColumn
{
    NONE,
    ROLL,
    NAME,
    GRADE,
    DEPARTMENT,
    CGPA
};
class SortState
{
public:
    SortColumn column = SortColumn::NONE;
    bool ascending = true;
};

class StudentManager
{
public:
    vector<Student> students;
    SortState sortState;

    void add(const Student &s) { students.push_back(s); }
    void removeByRoll(int roll)
    {
        students.erase(remove_if(students.begin(), students.end(),
                                 [roll](const Student &s)
                                 { return s.roll == roll; }),
                       students.end());
    }
    Student *findByRoll(int roll)
    {
        for (auto &s : students)
            if (s.roll == roll)
                return &s;
        return nullptr;
    }
    vector<Student *> search(const string &q)
    {
        vector<Student *> out;
        string lowerq = q;
        transform(lowerq.begin(), lowerq.end(), lowerq.begin(), ::tolower);
        for (auto &s : students)
        {
            string lname = s.name;
            transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
            string sroll = to_string(s.roll);
            if (lname.find(lowerq) != string::npos || sroll.find(lowerq) != string::npos)
                out.push_back(&s);
        }
        return out;
    }

    void sortBy(SortColumn column)
    {
        if (sortState.column == column)
        {
            // Same column - toggle direction
            sortState.ascending = !sortState.ascending;
        }
        else
        {
            // New column - default to ascending
            sortState.column = column;
            sortState.ascending = true;
        }

        // Perform sort
        switch (column)
        {
        case SortColumn::ROLL:
            sort(students.begin(), students.end(), [this](const Student &a, const Student &b)
                 { return sortState.ascending ? (a.roll < b.roll) : (a.roll > b.roll); });
            break;
        case SortColumn::NAME:
            sort(students.begin(), students.end(), [this](const Student &a, const Student &b)
                 { return sortState.ascending ? (a.name < b.name) : (a.name > b.name); });
            break;
        case SortColumn::GRADE:
            sort(students.begin(), students.end(), [this](const Student &a, const Student &b)
                 { return sortState.ascending ? (a.grade < b.grade) : (a.grade > b.grade); });
            break;
        case SortColumn::DEPARTMENT:
            sort(students.begin(), students.end(), [this](const Student &a, const Student &b)
                 { return sortState.ascending ? (a.department < b.department) : (a.department > b.department); });
            break;
        case SortColumn::CGPA:
            sort(students.begin(), students.end(), [this](const Student &a, const Student &b)
                 { return sortState.ascending ? (a.cgpa < b.cgpa) : (a.cgpa > b.cgpa); });
            break;
        default:
            break;
        }
    }

    void save(const string &fname = "students.txt")
    {
        ofstream f(fname);
        for (auto &s : students)
            f << s.name << '\t' << s.roll << '\t' << s.grade << '\t' << s.department << '\t' << s.cgpa << '\n';
    }
    void load(const string &fname = "students.txt")
    {
        students.clear();
        ifstream f(fname);
        if (!f.is_open())
            return;
        string line;
        while (getline(f, line))
        {
            if (line.empty())
                continue;
            stringstream ss(line);
            string name, grade, department;
            int roll;
            float cgpa;
            getline(ss, name, '\t');
            ss >> roll;
            ss.get();
            getline(ss, grade, '\t');
            getline(ss, department, '\t');
            ss >> cgpa;
            students.push_back({name, roll, grade, department, cgpa});
        }
    }
};

// ------------------------- Global Input -------------------------
static double mouseX = 0, mouseY = 0;
static bool mousePressed = false, mouseJustPressed = false;
static bool keysDown[1024] = {0};
static string textInputBuffer;
static double lastClickTime = 0.0;
static const double DOUBLE_CLICK_TIME = 0.3; // 300ms for double click

static void cursor_cb(GLFWwindow *w, double x, double y)
{
    mouseX = x;
    mouseY = y;
}
static void mouse_cb(GLFWwindow *w, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            mousePressed = true;
            mouseJustPressed = true;
        }
        else if (action == GLFW_RELEASE)
            mousePressed = false;
    }
}
static void key_cb(GLFWwindow *w, int key, int sc, int action, int mods)
{
    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS)
            keysDown[key] = true;
        if (action == GLFW_RELEASE)
            keysDown[key] = false;
    }
}
static void char_cb(GLFWwindow *w, unsigned int cp)
{
    if (cp >= 32 && cp < 128)
        textInputBuffer.push_back((char)cp);
    else if (cp == 8 && !textInputBuffer.empty())
        textInputBuffer.pop_back();
}

static bool pointInRect(float px, float py, float x, float y, float w, float h)
{
    return (px >= x && px <= x + w && py >= y && py <= y + h);
}

// ------------------------- Render Helpers -------------------------
void drawRect(float x, float y, float w, float h, float r, float g, float b, float alpha = 1.0f)
{
    glColor4f(r, g, b, alpha);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void drawOutline(float x, float y, float w, float h, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void drawText(float x, float y, const string &text, float r, float g, float b, int SCR_H, float scale = 2.0f, float alpha = 1.0f)
{
    static char buffer[99999];

    float flipped_y = SCR_H - y;

    int num_quads = stb_easy_font_print(x, flipped_y, (char *)text.c_str(), NULL, buffer, sizeof(buffer));

    glColor4f(r, g, b, alpha);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 800, 700, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Apply scaling for larger text
    glTranslatef(x, flipped_y, 0);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-x, -flipped_y, 0);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Helper function to pad string to fixed width
string padString(const string &str, int width)
{
    if (str.length() >= width)
        return str.substr(0, width);
    return str + string(width - str.length(), ' ');
}

// Draw centered popup message
void drawMessagePopup(const MessagePopup &popup, int SCR_W, int SCR_H, double currentTime)
{
    if (!popup.isVisible(currentTime))
        return;

    float alpha = popup.getAlpha(currentTime);

    // Popup dimensions - made larger to fit text better
    float popupW = 500;
    float popupH = 100;
    float popupX = (SCR_W - popupW) / 2.0f;
    float popupY = (SCR_H - popupH) / 2.0f;

    // Semi-transparent background overlay
    drawRect(0, 0, SCR_W, SCR_H, 0, 0, 0, 0.4f * alpha);

    // Popup box with shadow
    drawRect(popupX + 5, popupY - 5, popupW, popupH, 0, 0, 0, 0.3f * alpha);
    drawRect(popupX, popupY, popupW, popupH, 0.15f, 0.7f, 0.15f, alpha);
    drawOutline(popupX, popupY, popupW, popupH, 0.2f, 0.9f, 0.2f);

    // Center the text in the popup
    float textScale = 2.0f;
    float textX = popupX + 30;
    float textY = popupY + 40;

    drawText(textX, textY, popup.text, 1.0f, 1.0f, 1.0f, SCR_H, textScale, alpha);
}

// Draw details panel
void drawDetailsPanel(const DetailsPanel &panel, int SCR_W, int SCR_H, double currentTime)
{
    if (!panel.visible || !panel.currentStudent)
        return;

    float slideProgress = panel.getSlideProgress(currentTime);

    // Panel dimensions
    float panelW = 350;
    float panelH = SCR_H;
    float panelX = SCR_W - panelW * slideProgress;
    float panelY = 0;

    // Semi-transparent overlay
    drawRect(0, 0, SCR_W, SCR_H, 0, 0, 0, 0.3f * slideProgress);

    // Panel background with shadow
    drawRect(panelX - 10, panelY, 10, panelH, 0, 0, 0, 0.5f * slideProgress);
    drawRect(panelX, panelY, panelW, panelH, 0.14f, 0.15f, 0.16f, 1.0f);

    // Panel header
    drawRect(panelX, SCR_H - 80, panelW, 80, 0.2f, 0.3f, 0.5f, 1.0f);
    drawText(panelX + 20, SCR_H - 45, "Student Details", 1.0f, 1.0f, 1.0f, SCR_H, 2.0f);

    // Close button (X)
    float closeX = panelX + panelW - 50;
    float closeY = SCR_H - 60;
    drawRect(closeX, closeY, 35, 35, 0.7f, 0.3f, 0.3f, 1.0f);
    drawText(closeX + 10, closeY + 22, "X", 1.0f, 1.0f, 1.0f, SCR_H, 2.0f);

    // Student details
    Student *s = panel.currentStudent;
    float detailY = SCR_H - 130;
    float lineHeight = 70;

    // Name
    drawText(panelX + 20, detailY, "Name:", 0.7f, 0.7f, 0.7f, SCR_H, 1.5f);
    drawText(panelX + 20, detailY - 30, s->name, 1.0f, 1.0f, 1.0f, SCR_H, 1.7f);
    detailY -= lineHeight;

    // Roll
    drawText(panelX + 20, detailY, "Roll Number:", 0.7f, 0.7f, 0.7f, SCR_H, 1.5f);
    drawText(panelX + 20, detailY - 30, to_string(s->roll), 1.0f, 1.0f, 1.0f, SCR_H, 1.7f);
    detailY -= lineHeight;

    // Department
    drawText(panelX + 20, detailY, "Department:", 0.7f, 0.7f, 0.7f, SCR_H, 1.5f);
    drawText(panelX + 20, detailY - 30, s->department, 1.0f, 1.0f, 1.0f, SCR_H, 1.7f);
    detailY -= lineHeight;

    // Grade
    drawText(panelX + 20, detailY, "Grade:", 0.7f, 0.7f, 0.7f, SCR_H, 1.5f);
    drawText(panelX + 20, detailY - 30, s->grade, 1.0f, 1.0f, 1.0f, SCR_H, 1.7f);
    detailY -= lineHeight;

    // CGPA
    drawText(panelX + 20, detailY, "CGPA:", 0.7f, 0.7f, 0.7f, SCR_H, 1.5f);
    char cgpaStr[20];
    snprintf(cgpaStr, sizeof(cgpaStr), "%.2f / 4.00", s->cgpa);
    drawText(panelX + 20, detailY - 30, string(cgpaStr), 1.0f, 1.0f, 1.0f, SCR_H, 1.7f);
}

// ------------------------- Main -------------------------
int main()
{
    if (!glfwInit())
    {
        cerr << "GLFW init failed\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    int SCR_W = 1000, SCR_H = 700;
    GLFWwindow *window = glfwCreateWindow(SCR_W, SCR_H, "Student Management 2D GUI", NULL, NULL);
    if (!window)
    {
        cerr << "Window create failed\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, cursor_cb);
    glfwSetMouseButtonCallback(window, mouse_cb);
    glfwSetKeyCallback(window, key_cb);
    glfwSetCharCallback(window, char_cb);

    glViewport(0, 0, SCR_W, SCR_H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCR_W, 0, SCR_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ------------------------- UI Elements (FIXED POSITIONS) -------------------------
    // Buttons positioned 20px from top (in bottom-left coordinate system: SCR_H - 20 - 40)
    float btnY = (float)SCR_H - 60.0f; // 20px margin from top, 40px button height
    Button btnAdd{20.0f, btnY, 100.0f, 40.0f, "Add"};
    Button btnEdit{140.0f, btnY, 100.0f, 40.0f, "Edit"};
    Button btnDelete{260.0f, btnY, 100.0f, 40.0f, "Delete"};
    Button btnSave{380.0f, btnY, 100.0f, 40.0f, "Save"};
    Button btnLoad{500.0f, btnY, 100.0f, 40.0f, "Load"};

    // First row of input boxes
    float inputY1 = (float)SCR_H - 110.0f; // 70px from top (20px margin + 40px button + 10px gap)
    InputBox inputName{20.0f, inputY1, 200.0f, 35.0f, "", false};
    InputBox inputRoll{230.0f, inputY1, 100.0f, 35.0f, "", false};
    InputBox inputGrade{340.0f, inputY1, 100.0f, 35.0f, "", false};
    InputBox inputSearch{450.0f, inputY1, 510.0f, 35.0f, "", false};

    // Second row of input boxes
    float inputY2 = (float)SCR_H - 155.0f; // 45px below first row
    InputBox inputDepartment{20.0f, inputY2, 200.0f, 35.0f, "", false};
    InputBox inputCGPA{230.0f, inputY2, 100.0f, 35.0f, "", false};

    StudentManager manager;
    manager.load();
    Student *selected = nullptr;
    float scrollOffset = 0.0f;
    vector<int> selectedRolls; // Track selected student rolls for deletion

    // Message popup
    MessagePopup messagePopup;

    // Details panel
    DetailsPanel detailsPanel;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        double currentTime = glfwGetTime();

        // Mouse conversion (top-left origin to bottom-left)
        double mx = mouseX, my = mouseY;
        my = SCR_H - my;
        bool click = false;
        bool doubleClick = false;
        if (mouseJustPressed)
        {
            click = true;
            mouseJustPressed = false;

            // Check for double click
            if (currentTime - lastClickTime <= DOUBLE_CLICK_TIME)
            {
                doubleClick = true;
            }
            lastClickTime = currentTime;
        }

        // Handle clicks
        if (click)
        {
            // Check if clicking close button on details panel
            if (detailsPanel.visible)
            {
                float panelW = 350;
                float panelX = SCR_W - panelW;
                float closeX = panelX + panelW - 50;
                float closeY = SCR_H - 60;

                if (pointInRect((float)mx, (float)my, closeX, closeY, 35, 35))
                {
                    detailsPanel.hide();
                    click = false; // Consume the click
                }
                // Check if clicking outside panel to close
                else if (mx < panelX)
                {
                    detailsPanel.hide();
                    click = false;
                }
            }

            if (click) // Only process other clicks if not consumed by panel
            {
                inputName.focused = pointInRect((float)mx, (float)my, inputName.x, inputName.y, inputName.w, inputName.h);
                inputRoll.focused = pointInRect((float)mx, (float)my, inputRoll.x, inputRoll.y, inputRoll.w, inputRoll.h);
                inputGrade.focused = pointInRect((float)mx, (float)my, inputGrade.x, inputGrade.y, inputGrade.w, inputGrade.h);
                inputSearch.focused = pointInRect((float)mx, (float)my, inputSearch.x, inputSearch.y, inputSearch.w, inputSearch.h);
                inputDepartment.focused = pointInRect((float)mx, (float)my, inputDepartment.x, inputDepartment.y, inputDepartment.w, inputDepartment.h);
                inputCGPA.focused = pointInRect((float)mx, (float)my, inputCGPA.x, inputCGPA.y, inputCGPA.w, inputCGPA.h);

                // Check for column header clicks for sorting
                float listX = 20, listTop = SCR_H - 225;
                float headerX = listX + 10;
                float headerY = listTop - 20;
                float headerH = 25;

                bool headerClicked = false;

                // Check columns from right to left with exact boundaries
                if (pointInRect((float)mx, (float)my, headerX + 620, headerY - headerH, 150, headerH))
                {
                    // CGPA column clicked (rightmost)
                    manager.sortBy(SortColumn::CGPA);
                    headerClicked = true;
                }
                if (!headerClicked && pointInRect((float)mx, (float)my, headerX + 520, headerY - headerH, 95, headerH))
                {
                    // Grade column clicked
                    manager.sortBy(SortColumn::GRADE);
                    headerClicked = true;
                }
                if (!headerClicked && pointInRect((float)mx, (float)my, headerX + 320, headerY - headerH, 195, headerH))
                {
                    // Department column clicked
                    manager.sortBy(SortColumn::DEPARTMENT);
                    headerClicked = true;
                }
                if (!headerClicked && pointInRect((float)mx, (float)my, headerX + 80, headerY - headerH, 235, headerH))
                {
                    // Name column clicked
                    manager.sortBy(SortColumn::NAME);
                    headerClicked = true;
                }
                if (!headerClicked && pointInRect((float)mx, (float)my, headerX, headerY - headerH, 75, headerH))
                {
                    // Roll column clicked
                    manager.sortBy(SortColumn::ROLL);
                    headerClicked = true;
                }

                auto hit = [&](const Button &b)
                { return pointInRect((float)mx, (float)my, b.x, b.y, b.w, b.h); };
                if (hit(btnAdd))
                {
                    btnAdd.pressed = true;
                    btnAdd.pressTime = currentTime;

                    int roll = -1;
                    try
                    {
                        roll = stoi(inputRoll.text);
                    }
                    catch (...)
                    {
                        roll = -1;
                    }
                    float cgpa = 0.0f;
                    try
                    {
                        cgpa = stof(inputCGPA.text);
                        if (cgpa > 4.0f)
                            cgpa = 4.0f; // Cap at 4.0
                        if (cgpa < 0.0f)
                            cgpa = 0.0f; // Minimum 0.0
                    }
                    catch (...)
                    {
                        cgpa = 0.0f;
                    }
                    if (!inputName.text.empty() && roll >= 0)
                    {
                        manager.add({inputName.text, roll, inputGrade.text, inputDepartment.text, cgpa});
                        inputName.text.clear();
                        inputRoll.text.clear();
                        inputGrade.text.clear();
                        inputDepartment.text.clear();
                        inputCGPA.text.clear();

                        // Show success message
                        messagePopup.show("Student added successfully!", currentTime);
                    }
                }
                else if (hit(btnEdit))
                {
                    btnEdit.pressed = true;
                    btnEdit.pressTime = currentTime;

                    int roll = -1;
                    try
                    {
                        roll = stoi(inputRoll.text);
                    }
                    catch (...)
                    {
                        roll = -1;
                    }
                    if (roll >= 0)
                    {
                        Student *s = manager.findByRoll(roll);
                        if (s)
                        {
                            s->name = inputName.text;
                            s->grade = inputGrade.text;
                            s->department = inputDepartment.text;
                            try
                            {
                                s->cgpa = stof(inputCGPA.text);
                                if (s->cgpa > 4.0f)
                                    s->cgpa = 4.0f;
                                if (s->cgpa < 0.0f)
                                    s->cgpa = 0.0f;
                            }
                            catch (...)
                            {
                                s->cgpa = 0.0f;
                            }

                            // Show success message
                            messagePopup.show("Student updated successfully!", currentTime);
                        }
                    }
                }
                else if (hit(btnDelete))
                {
                    btnDelete.pressed = true;
                    btnDelete.pressTime = currentTime;

                    bool deleted = false;
                    int deleteCount = 0;

                    // Delete all selected students
                    if (!selectedRolls.empty())
                    {
                        deleteCount = selectedRolls.size();
                        for (int roll : selectedRolls)
                        {
                            manager.removeByRoll(roll);
                        }
                        selectedRolls.clear();
                        // AUTO-SAVE: Save to file after deletion
                        manager.save();
                        deleted = true;
                    }
                    else
                    {
                        // If nothing selected, try to delete by Roll input field (original behavior)
                        int roll = -1;
                        try
                        {
                            roll = stoi(inputRoll.text);
                        }
                        catch (...)
                        {
                            roll = -1;
                        }
                        if (roll >= 0)
                        {
                            manager.removeByRoll(roll);
                            // AUTO-SAVE: Save to file after deletion
                            manager.save();
                            deleteCount = 1;
                            deleted = true;
                        }
                    }

                    // Show success message
                    if (deleted)
                    {
                        if (deleteCount > 1)
                            messagePopup.show("Students deleted successfully!", currentTime);
                        else
                            messagePopup.show("Student deleted successfully!", currentTime);
                    }
                }
                else if (hit(btnSave))
                {
                    btnSave.pressed = true;
                    btnSave.pressTime = currentTime;

                    manager.save();
                    // Show success message
                    messagePopup.show("Students saved successfully!", currentTime);
                }
                else if (hit(btnLoad))
                {
                    btnLoad.pressed = true;
                    btnLoad.pressTime = currentTime;

                    manager.load();
                    // Show success message
                    messagePopup.show("Students loaded successfully!", currentTime);
                }
            }
        }

        // Keyboard input
        if (!textInputBuffer.empty())
        {
            if (inputName.focused)
                inputName.text += textInputBuffer;
            else if (inputRoll.focused)
            {
                for (char c : textInputBuffer)
                    if (isdigit(c) || c == '-')
                        inputRoll.text.push_back(c);
            }
            else if (inputGrade.focused)
                inputGrade.text += textInputBuffer;
            else if (inputSearch.focused)
                inputSearch.text += textInputBuffer;
            else if (inputDepartment.focused)
                inputDepartment.text += textInputBuffer;
            else if (inputCGPA.focused)
            {
                for (char c : textInputBuffer)
                    if (isdigit(c) || c == '.')
                        inputCGPA.text.push_back(c);
            }
            textInputBuffer.clear();
        }

        if (keysDown[GLFW_KEY_BACKSPACE])
        {
            if (inputName.focused && !inputName.text.empty())
                inputName.text.pop_back();
            else if (inputRoll.focused && !inputRoll.text.empty())
                inputRoll.text.pop_back();
            else if (inputGrade.focused && !inputGrade.text.empty())
                inputGrade.text.pop_back();
            else if (inputSearch.focused && !inputSearch.text.empty())
                inputSearch.text.pop_back();
            else if (inputDepartment.focused && !inputDepartment.text.empty())
                inputDepartment.text.pop_back();
            else if (inputCGPA.focused && !inputCGPA.text.empty())
                inputCGPA.text.pop_back();
            keysDown[GLFW_KEY_BACKSPACE] = false;
        }
        if (keysDown[GLFW_KEY_ESCAPE])
        {
            if (detailsPanel.visible)
            {
                detailsPanel.hide();
                keysDown[GLFW_KEY_ESCAPE] = false;
            }
            else
            {
                break;
            }
        }

        // Prepare visible list
        vector<Student *> visible;
        if (!inputSearch.text.empty())
            visible = manager.search(inputSearch.text);
        else
            for (auto &s : manager.students)
                visible.push_back(&const_cast<Student &>(s));

        // Handle row clicks for both selection (double-click) and details view (single click)
        if (click)
        {
            float listX = 20, listTop = SCR_H - 225, listW = SCR_W - 40;
            float ystart = listTop - 50 - scrollOffset;
            int idx = 0;
            for (auto *s : visible)
            {
                float itemY = ystart - idx * 24;
                if (itemY > 30 && itemY < listTop - 30)
                {
                    // Check if click is within this row
                    if (pointInRect((float)mx, (float)my, listX + 5, itemY - 18, listW - 10, 20))
                    {
                        // Single click - show details panel
                        detailsPanel.show(s, currentTime);
                        break;
                    }
                }
                ++idx;
            }
        }

        // Handle row selection with double-click
        if (doubleClick)
        {
            float listX = 20, listTop = SCR_H - 225, listW = SCR_W - 40;
            float ystart = listTop - 50 - scrollOffset;
            int idx = 0;
            for (auto *s : visible)
            {
                float itemY = ystart - idx * 24;
                if (itemY > 30 && itemY < listTop - 30)
                {
                    // Check if double-click is within this row
                    if (pointInRect((float)mx, (float)my, listX + 5, itemY - 18, listW - 10, 20))
                    {
                        // Toggle selection
                        auto it = find(selectedRolls.begin(), selectedRolls.end(), s->roll);
                        if (it != selectedRolls.end())
                        {
                            // Already selected, deselect it
                            selectedRolls.erase(it);
                        }
                        else
                        {
                            // Not selected, select it
                            selectedRolls.push_back(s->roll);
                        }
                        break;
                    }
                }
                ++idx;
            }
        }

        // ------------------------- Rendering -------------------------
        glClearColor(0.06f, 0.07f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Top bar background (increased height for second row)
        drawRect(0, SCR_H - 205, SCR_W, 205, 0.1f, 0.11f, 0.12f);

        // Helper to check if button should show press effect
        auto isButtonPressed = [&](const Button &btn)
        {
            return btn.pressed && (currentTime - btn.pressTime < 0.2);
        };

        // Buttons with press effect
        float addOffset = isButtonPressed(btnAdd) ? 2.0f : 0.0f;
        drawRect(btnAdd.x, btnAdd.y - addOffset, btnAdd.w, btnAdd.h,
                 isButtonPressed(btnAdd) ? 0.15f : 0.2f,
                 isButtonPressed(btnAdd) ? 0.5f : 0.6f,
                 isButtonPressed(btnAdd) ? 0.15f : 0.2f);

        float editOffset = isButtonPressed(btnEdit) ? 2.0f : 0.0f;
        drawRect(btnEdit.x, btnEdit.y - editOffset, btnEdit.w, btnEdit.h,
                 isButtonPressed(btnEdit) ? 0.15f : 0.2f,
                 isButtonPressed(btnEdit) ? 0.4f : 0.5f,
                 isButtonPressed(btnEdit) ? 0.7f : 0.8f);

        float deleteOffset = isButtonPressed(btnDelete) ? 2.0f : 0.0f;
        drawRect(btnDelete.x, btnDelete.y - deleteOffset, btnDelete.w, btnDelete.h,
                 isButtonPressed(btnDelete) ? 0.7f : 0.8f,
                 isButtonPressed(btnDelete) ? 0.25f : 0.3f,
                 isButtonPressed(btnDelete) ? 0.25f : 0.3f);

        float saveOffset = isButtonPressed(btnSave) ? 2.0f : 0.0f;
        drawRect(btnSave.x, btnSave.y - saveOffset, btnSave.w, btnSave.h,
                 isButtonPressed(btnSave) ? 0.6f : 0.7f,
                 isButtonPressed(btnSave) ? 0.5f : 0.6f,
                 isButtonPressed(btnSave) ? 0.15f : 0.2f);

        float loadOffset = isButtonPressed(btnLoad) ? 2.0f : 0.0f;
        drawRect(btnLoad.x, btnLoad.y - loadOffset, btnLoad.w, btnLoad.h,
                 isButtonPressed(btnLoad) ? 0.4f : 0.5f,
                 isButtonPressed(btnLoad) ? 0.4f : 0.5f,
                 isButtonPressed(btnLoad) ? 0.4f : 0.5f);

        // Button labels - centered in buttons with larger font and adjusted for offset
        float btnScale = 1.5f;
        float textCenterY = 25.0f;
        drawText(btnAdd.x + 22, btnAdd.y + textCenterY - addOffset, btnAdd.label, 1, 1, 1, SCR_H, btnScale);
        drawText(btnEdit.x + 5, btnEdit.y + textCenterY - editOffset, btnEdit.label, 1, 1, 1, SCR_H, btnScale);
        drawText(btnDelete.x - 35, btnDelete.y + textCenterY - deleteOffset, btnDelete.label, 1, 1, 1, SCR_H, btnScale);
        drawText(btnSave.x - 50, btnSave.y + textCenterY - saveOffset, btnSave.label, 1, 1, 1, SCR_H, btnScale);
        drawText(btnLoad.x - 80, btnLoad.y + textCenterY - loadOffset, btnLoad.label, 1, 1, 1, SCR_H, btnScale);

        // Input boxes - First row with focused color change
        drawRect(inputName.x, inputName.y, inputName.w, inputName.h,
                 inputName.focused ? 0.9f : 1.0f,
                 inputName.focused ? 0.95f : 1.0f,
                 inputName.focused ? 1.0f : 1.0f);
        drawRect(inputRoll.x, inputRoll.y, inputRoll.w, inputRoll.h,
                 inputRoll.focused ? 0.9f : 1.0f,
                 inputRoll.focused ? 0.95f : 1.0f,
                 inputRoll.focused ? 1.0f : 1.0f);
        drawRect(inputGrade.x, inputGrade.y, inputGrade.w, inputGrade.h,
                 inputGrade.focused ? 0.9f : 1.0f,
                 inputGrade.focused ? 0.95f : 1.0f,
                 inputGrade.focused ? 1.0f : 1.0f);
        drawRect(inputSearch.x, inputSearch.y, inputSearch.w, inputSearch.h,
                 inputSearch.focused ? 0.9f : 1.0f,
                 inputSearch.focused ? 0.95f : 1.0f,
                 inputSearch.focused ? 1.0f : 1.0f);

        drawOutline(inputName.x, inputName.y, inputName.w, inputName.h, inputName.focused ? 0.1f : 0.25f, inputName.focused ? 0.6f : 0.25f, inputName.focused ? 0.9f : 0.25f);
        drawOutline(inputRoll.x, inputRoll.y, inputRoll.w, inputRoll.h, inputRoll.focused ? 0.1f : 0.25f, inputRoll.focused ? 0.6f : 0.25f, inputRoll.focused ? 0.9f : 0.25f);
        drawOutline(inputGrade.x, inputGrade.y, inputGrade.w, inputGrade.h, inputGrade.focused ? 0.1f : 0.25f, inputGrade.focused ? 0.6f : 0.25f, inputGrade.focused ? 0.9f : 0.25f);
        drawOutline(inputSearch.x, inputSearch.y, inputSearch.w, inputSearch.h, inputSearch.focused ? 0.1f : 0.25f, inputSearch.focused ? 0.6f : 0.25f, inputSearch.focused ? 0.9f : 0.25f);

        drawText(inputName.x + 8, inputName.y + 20, inputName.text.empty() ? "Name..." : inputName.text, 0, 0, 0, SCR_H, 1.5f);
        drawText(inputRoll.x - 30, inputRoll.y + 20, inputRoll.text.empty() ? "Roll..." : inputRoll.text, 0, 0, 0, SCR_H, 1.5f);
        drawText(inputGrade.x - 60, inputGrade.y + 20, inputGrade.text.empty() ? "Grade..." : inputGrade.text, 0, 0, 0, SCR_H, 1.5f);
        drawText(inputSearch.x + 8, inputSearch.y + 20, inputSearch.text.empty() ? "Search name/roll..." : inputSearch.text, 0.4f, 0.4f, 0.4f, SCR_H, 1.5f);

        // Input boxes - Second row with focused color change
        drawRect(inputDepartment.x, inputDepartment.y, inputDepartment.w, inputDepartment.h,
                 inputDepartment.focused ? 0.9f : 1.0f,
                 inputDepartment.focused ? 0.95f : 1.0f,
                 inputDepartment.focused ? 1.0f : 1.0f);
        drawRect(inputCGPA.x, inputCGPA.y, inputCGPA.w, inputCGPA.h,
                 inputCGPA.focused ? 0.9f : 1.0f,
                 inputCGPA.focused ? 0.95f : 1.0f,
                 inputCGPA.focused ? 1.0f : 1.0f);

        drawOutline(inputDepartment.x, inputDepartment.y, inputDepartment.w, inputDepartment.h, inputDepartment.focused ? 0.1f : 0.25f, inputDepartment.focused ? 0.6f : 0.25f, inputDepartment.focused ? 0.9f : 0.25f);
        drawOutline(inputCGPA.x, inputCGPA.y, inputCGPA.w, inputCGPA.h, inputCGPA.focused ? 0.1f : 0.25f, inputCGPA.focused ? 0.6f : 0.25f, inputCGPA.focused ? 0.9f : 0.25f);

        drawText(inputDepartment.x + 8, inputDepartment.y + 20, inputDepartment.text.empty() ? "Department..." : inputDepartment.text, 0, 0, 0, SCR_H, 1.5f);
        drawText(inputCGPA.x - 30, inputCGPA.y + 20, inputCGPA.text.empty() ? "CGPA..." : inputCGPA.text, 0, 0, 0, SCR_H, 1.5f);

        // List background
        float listX = 20, listTop = SCR_H - 225, listW = SCR_W - 40, listH = SCR_H - 245;
        drawRect(listX, 20, listW, listH, 0.12f, 0.13f, 0.14f);
        drawOutline(listX, 20, listW, listH, 0.2f, 0.2f, 0.2f);

        // List header - with Department and CGPA columns (clickable for sorting)
        float headerX = listX + 10;
        float headerY = listTop - 20;

        // Draw column headers with sort indicators
        string rollHeader = "Roll";
        string nameHeader = "Name";
        string deptHeader = "Department";
        string gradeHeader = "Grade";
        string cgpaHeader = "CGPA";

        // Add sort indicators (^ for ascending, v for descending)
        if (manager.sortState.column == SortColumn::ROLL)
        {
            rollHeader += (manager.sortState.ascending ? " ^" : " v");
        }
        else if (manager.sortState.column == SortColumn::NAME)
        {
            nameHeader += (manager.sortState.ascending ? " ^" : " v");
        }
        else if (manager.sortState.column == SortColumn::DEPARTMENT)
        {
            deptHeader += (manager.sortState.ascending ? " ^" : " v");
        }
        else if (manager.sortState.column == SortColumn::GRADE)
        {
            gradeHeader += (manager.sortState.ascending ? " ^" : " v");
        }
        else if (manager.sortState.column == SortColumn::CGPA)
        {
            cgpaHeader += (manager.sortState.ascending ? " ^" : " v");
        }

        drawText(headerX, headerY, rollHeader, 0.8f, 0.8f, 0.8f, SCR_H, 1.4f);
        drawText(headerX + 80, headerY, nameHeader, 0.8f, 0.8f, 0.8f, SCR_H, 1.4f);
        drawText(headerX + 320, headerY, deptHeader, 0.8f, 0.8f, 0.8f, SCR_H, 1.4f);
        drawText(headerX + 520, headerY, gradeHeader, 0.8f, 0.8f, 0.8f, SCR_H, 1.4f);
        drawText(headerX + 620, headerY, cgpaHeader, 0.8f, 0.8f, 0.8f, SCR_H, 1.4f);

        // List items - with Department and CGPA columns
        float ystart = listTop - 50 - scrollOffset;
        int idx = 0;
        for (auto *s : visible)
        {
            float itemY = ystart - idx * 24;
            if (itemY > 30 && itemY < listTop - 30)
            { // Only draw visible items
                // Check if this student is selected
                bool isSelected = find(selectedRolls.begin(), selectedRolls.end(), s->roll) != selectedRolls.end();

                // Draw background with selection highlight
                if (isSelected)
                    drawRect(listX + 5, itemY - 18, listW - 10, 20, 0.2f, 0.4f, 0.6f); // Blue highlight for selected
                else
                    drawRect(listX + 5, itemY - 18, listW - 10, 20, 0.15f, 0.16f, 0.17f);

                // Draw each column at fixed positions
                float dataX = listX + 12;
                drawText(dataX, itemY - 2, to_string(s->roll), 0.9f, 0.9f, 0.9f, SCR_H, 1.3f);
                drawText(dataX + 80, itemY - 2, s->name, 0.9f, 0.9f, 0.9f, SCR_H, 1.3f);
                drawText(dataX + 320, itemY - 2, s->department, 0.9f, 0.9f, 0.9f, SCR_H, 1.3f);
                drawText(dataX + 520, itemY - 2, s->grade, 0.9f, 0.9f, 0.9f, SCR_H, 1.3f);

                // Format CGPA to 2 decimal places
                char cgpaStr[10];
                snprintf(cgpaStr, sizeof(cgpaStr), "%.2f", s->cgpa);
                drawText(dataX + 620, itemY - 2, string(cgpaStr), 0.9f, 0.9f, 0.9f, SCR_H, 1.3f);
            }
            ++idx;
        }

        // Draw message popup (on top of everything except details panel)
        drawMessagePopup(messagePopup, SCR_W, SCR_H, currentTime);

        // Draw details panel (on top of everything)
        drawDetailsPanel(detailsPanel, SCR_W, SCR_H, currentTime);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}