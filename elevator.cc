#include <queue>
#include <vector>
#include <functional>
#include <stdexcept>
#include <assert.h>
#include <fmt/core.h>

const int NUM_ELEVATORS = 2;
const int NUM_FLOORS = 5;

// above and below refer to the position of the person relative to the elevator
// and up and down refer to which direction they want to go
enum direction {
    up,
    down,
    neutral,
    retrieval_above_going_up,
    retrieval_above_going_down,
    retrieval_below_going_up,
    retrieval_below_going_down
};

// if the value is i, then the elevator is either on floor i or going to floor i
int floors[NUM_ELEVATORS];
direction directions[NUM_ELEVATORS];

int destinations[NUM_ELEVATORS];

// max heap for lower floors and min heap for higher floors
// we need one priority queue per elevator since these are for inside buttons
std::priority_queue<int, std::vector<int>, std::greater<int>> higher_floors_pressed[NUM_ELEVATORS];
std::priority_queue<int> lower_floors_pressed[NUM_ELEVATORS];

// arrays for external buttons
bool up_button[NUM_FLOORS];
bool down_button[NUM_FLOORS];

// unpress all external and internal buttons, reset directions, and move all
// elevators to first floor
void reset() {
    for (int i = 0; i < NUM_FLOORS; i++) {
        up_button[i] = false;
        down_button[i] = false;
    }

    for (int i = 0; i < NUM_ELEVATORS; i++) {
        move_to_bottom_floor(i);
        directions[i] = neutral;
        destinations[i] = -1;
        while (!higher_floors_pressed[i].empty()) higher_floors_pressed[i].pop();
        while (!lower_floors_pressed[i].empty()) lower_floors_pressed[i].pop();
    }
}

void press_outside(int floor, direction dir) {
    assert((floor >= 0 && floor < NUM_FLOORS) && fmt::format("there is no floor {}", floor).c_str());
    switch (dir) {
        case up:
            if (floor == NUM_FLOORS - 1) {
                throw std::invalid_argument("there is no up button on topmost floor");
            } else {
                up_button[dir] = true;
            }
            break;
        case down:
            if (floor == 0) {
                throw std::invalid_argument("there is no down button on floor 1");
            } else {
                down_button[dir] = true;
            }
            break;
        case retrieval_above_going_up:
        case retrieval_above_going_down:
        case retrieval_below_going_up:
        case retrieval_below_going_down:
        case neutral:
            throw std::invalid_argument("there is no button for the neutral and retrieval cases");
            break;
    }
}

void press_inside(int elevator, int floor) {
    assert((floor >= 0 && floor < NUM_FLOORS) && fmt::format("there is no floor {}", floor + 1).c_str());
    assert((elevator >= 0 && elevator < NUM_ELEVATORS) &&
        fmt::format("there is no elevator {}", elevator + 1).c_str());
    // there isn't a good behavior with people inside in these cases so just switch to neutral
    if (directions[elevator] == retrieval_above_going_down || directions[elevator] == retrieval_below_going_up) {
        directions[elevator] = neutral;
    }
    if (floor > floors[elevator]) {
        higher_floors_pressed[elevator].push(floor);
    } else if (floor < floors[elevator]) {
        lower_floors_pressed[elevator].push(floor);
    } else {
        switch (directions[elevator]) {
            case up:
            case retrieval_above_going_up:
                higher_floors_pressed[elevator].push(floor);
                break;
            case down:
            case retrieval_below_going_down:
                lower_floors_pressed[elevator].push(floor);
                break;
            // doesn't actually matter if it goes to up or down
            case neutral:
                directions[elevator] = up;
                higher_floors_pressed[elevator].push(floor);
                break;
        }
    }
}

void open(int elevator) {
    assert((elevator >= 0 && elevator < NUM_ELEVATORS) &&
        fmt::format("there is no elevator {}", elevator + 1).c_str());
    open_door(elevator);
    int floor = floors[elevator];
    if (top_of_pq_equal_floor(higher_floors_pressed[elevator], floor)) higher_floors_pressed[elevator].pop();
    if (top_of_pq_equal_floor(lower_floors_pressed[elevator], floor)) lower_floors_pressed[elevator].pop();
    switch (directions[elevator]) {
        case up:
        case retrieval_above_going_up:
        case retrieval_below_going_up:
            unpress_external_button(up, floor);
            break;
        case down:
        case retrieval_above_going_down:
        case retrieval_below_going_down:
            unpress_external_button(down, floor);
            break;
        case neutral:
            throw std::invalid_argument("the elevator shouldn't open while in neutral");
            break;
    }
    close_door(elevator);
}

// each elevator will alternate between moving and opening, so all logic related to
// figuring out where to go and how to get there should be handled within this loop
void move(int elevator) {
    bool should_open_now = false;
    while (!should_open_now) {
        int floor = floors[elevator];
        switch (directions[elevator]) {
            // open door cause someone wants to get in or out, switch to neutral cause no one
            // else inside wants to go up, or keep going up because there's no reason to stop
            case up:
                if (up_button[floor] || top_of_pq_equal_floor(higher_floors_pressed[elevator], floor)) {
                    should_open_now = true;
                } else if (higher_floors_pressed[elevator].empty()) {
                    directions[elevator] = neutral;
                } else move_up_one_floor(elevator);
                break;
            // open if you've got to your destination or if someone outside wants to go up or if someone inside 
            // wants to get out, switch to neutral if the person no longer wants to go up, or keep going up
            case retrieval_above_going_up:
                if (up_button[floor] || destinations[elevator] == floor ||
                    top_of_pq_equal_floor(higher_floors_pressed[elevator], floor)) {
                    should_open_now = true;
                    if (destinations[elevator] == floor) {
                        destinations[elevator] = -1;
                        directions[elevator] = up;
                    }
                } else if (!up_button[destinations[elevator]]) {
                    directions[elevator] = neutral;
                    destinations[elevator] = -1;
                } else move_up_one_floor(elevator);
                break;
            // switch to neutral if the person no longer wants to go up, open door if
            // you're at the target floor, or keep going down to destination
            case retrieval_below_going_up:
                if (!up_button[destinations[elevator]]) {
                    directions[elevator] = neutral;
                    destinations[elevator] = -1;
                } else if (destinations[elevator] == floor) {
                    should_open_now = true;
                    destinations[elevator] = -1;
                    directions[elevator] = up;
                } else move_down_one_floor(elevator);
                break;
            // mirrors up
            case down:
                if (down_button[floor] || top_of_pq_equal_floor(lower_floors_pressed[elevator], floor)) {
                    should_open_now = true;
                } else if (lower_floors_pressed[elevator].empty()) {
                    directions[elevator] = neutral;
                } else move_down_one_floor(elevator);
                break;
            // mirrors retrieval below going up
            case retrieval_above_going_down:
                if (!down_button[destinations[elevator]]) {
                    directions[elevator] = neutral;
                    destinations[elevator] = -1;
                } else if (destinations[elevator] == floor) {
                    should_open_now = true;
                    destinations[elevator] = -1;
                    directions[elevator] = down;
                } else move_up_one_floor(elevator);
                break;
            // mirrors retrieval above going up
            case retrieval_below_going_down:
                if (down_button[floor] || destinations[elevator] == floor ||
                    top_of_pq_equal_floor(lower_floors_pressed[elevator], floor)) {
                    should_open_now = true;
                    if (destinations[elevator] == floor) {
                        destinations[elevator] = -1;
                        directions[elevator] = down;
                    }
                } else if (!down_button[destinations[elevator]]) {
                    directions[elevator] = neutral;
                    destinations[elevator] = -1;
                } else move_down_one_floor(elevator);
                break;
            case neutral:
                bool searching = true;
                while (searching) {
                    // go into up or down mode if any internal buttons are pressed
                    if (!higher_floors_pressed[elevator].empty() || !lower_floors_pressed[elevator].empty()) {
                        int higher_count = higher_floors_pressed[elevator].size();
                        int lower_count = lower_floors_pressed[elevator].size();
                        directions[elevator] = (higher_count > lower_count) ? up : down;
                        searching = false;
                        break;
                    }
                    // if no internal buttons are pressed, then look for an external button in order
                    // to retrieve someone who's waiting
                    for (int i = 1; i < NUM_FLOORS; i++) {
                        if (floor + i < NUM_FLOORS && (up_button[floor + i] || down_button[floor + i])) {
                            bool go_up = up_button[floor + i];
                            directions[elevator] = go_up ? retrieval_above_going_up : retrieval_above_going_down;
                            destinations[elevator] = floor + i;
                            searching = false;
                            break;
                        }
                        if (floor - i >= 0 && (up_button[floor - i] || down_button[floor - i])) {
                            bool go_up = up_button[floor - i];
                            directions[elevator] = go_up ? retrieval_below_going_up : retrieval_below_going_down;
                            destinations[elevator] = floor - i;
                            searching = false;
                            break;
                        }
                    }
                }
                break;
        }
    }
}

void move_up_one_floor(int elevator) {
    assert(floors[elevator] < NUM_FLOORS - 1);
    floors[elevator]++;
    printf(fmt::format("elevator {} is now on floor {}", elevator + 1, floors[elevator] + 1).c_str());
}

void move_down_one_floor(int elevator) {
    assert(floors[elevator] > 0);
    floors[elevator]--;
    printf(fmt::format("elevator {} is now on floor {}", elevator + 1, floors[elevator] + 1).c_str());
}

void move_to_bottom_floor(int elevator) {
    floors[elevator] = 0;
    printf(fmt::format("elevator {} is now on floor {}", elevator + 1, floors[elevator] + 1).c_str());
}

void open_door(int elevator) {
    assert((elevator >= 0 && elevator < NUM_ELEVATORS) &&
        fmt::format("there is no elevator {}", elevator + 1).c_str());
    printf("opened door on floor %d\n", floors[elevator] + 1);
}

void close_door(int elevator) {
    assert((elevator >= 0 && elevator < NUM_ELEVATORS) &&
        fmt::format("there is no elevator {}", elevator + 1).c_str());
    printf("closed door on floor %d\n", floors[elevator] + 1);
}

// unpress external button if it is pressed
void unpress_external_button(direction dir, int floor) {
    assert(dir == up || dir == down);
    if (dir == up) {
        if (up_button[floor]) printf("up button on floor %d is unpressed \n", floor + 1);
        up_button[floor] = false;
    } else {
        if (down_button[floor]) printf("down button on floor %d is unpressed \n", floor + 1);
        down_button[floor] = false;
    }
}

// FIND A WAY TO REMOVE DUPLICATE IMPLEMENTATIONS IF YOU CAN
inline bool top_of_pq_equal_floor(std::priority_queue<int, std::vector<int>, std::greater<int>> pq, int floor) {
    return !pq.empty() && pq.top() == floor;
}

inline bool top_of_pq_equal_floor(std::priority_queue<int> pq, int floor) {
    return !pq.empty() && pq.top() == floor;
}