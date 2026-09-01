import pygame
import numpy as np

# Initialize Pygame and mixer module
pygame.init()
pygame.display.set_caption("""Theo"s Sorting app""")
pygame.mixer.init(frequency=44100, size=-16, channels=2)
sorting = False

# Set the display size
width, height = 1500, 850
screen = pygame.display.set_mode((width, height))

# Define colors
WHITE = (255, 255, 255)
BLUE = (0, 0, 255)
GRAY = (200, 200, 200)
DARK_GRAY = (50, 50, 50)
GREEN = (0, 255, 0)
RED = (255, 0, 0)

# Button properties
button_width = 150
button_height = 25
button_margin = 10

# Set the array size
speed = 90  # 0-100
size = 100
array = np.random.rand(size) * (height - button_height - 2 * button_margin)
bar_width = (width - 200) // size


def wait_for_speed():
    pygame.time.wait(max(0, 100 - speed))


def safe_max(arr):
    return max(float(max(arr)), 1.0) if len(arr) else 1.0


def handle_abort_events():
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            raise SystemExit
        if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
            return True
    return False


# Function to draw the array values as bars
def draw_array(arr, hl1=-1, hl2=-1):
    pygame.draw.rect(screen, WHITE, (0, 0, width, height))
    if isinstance(hl1, (list, tuple, set)):
        highlights = list(hl1)
        hl1 = highlights[0] if len(highlights) > 0 else -1
        hl2 = highlights[1] if len(highlights) > 1 else -1
    for i, val in enumerate(arr):
        val = max(0, float(val))
        if i == hl1:
            pygame.draw.rect(screen, GREEN,
                             (i * bar_width, height - val - (button_height + 2 * button_margin), bar_width, val))
        elif i == hl2:
            pygame.draw.rect(screen, RED,
                             (i * bar_width, height - val - (button_height + 2 * button_margin), bar_width, val))
        else:
            pygame.draw.rect(screen, BLUE,
                             (i * bar_width, height - val - (button_height + 2 * button_margin), bar_width, val))


def generate_tone(frequency, duration, fade_in_duration, fade_out_duration):
    sample_rate = 44100
    t = np.linspace(0, duration, int(sample_rate * duration))
    wave = np.sin(2 * np.pi * frequency * t)

    # Apply a fade in to the first part of the wave
    fade_in_samples = int(fade_in_duration * sample_rate)
    fade_in = np.linspace(0, 1, fade_in_samples)
    wave[:fade_in_samples] *= fade_in

    # Apply a fade out to the last part of the wave
    fade_out_samples = int(fade_out_duration * sample_rate)
    fade_out = np.linspace(1, 0, fade_out_samples)
    wave[-fade_out_samples:] *= fade_out

    # Ensure the wave is in the range [-1, 1] and return
    return wave.astype(np.float32)


def play_tone(frequency, duration=0.1, fade_in_duration=0.04, fade_out_duration=0.04):
    frequency = max(20, float(frequency))
    sound_wave = generate_tone(frequency, duration, fade_in_duration, fade_out_duration)
    sound_wave = np.array(sound_wave * 32767, 'int16')  # Scale to int16 for audio
    stereo_wave = np.vstack((sound_wave, sound_wave)).T
    # Ensure the array is C-contiguous
    stereo_wave = np.ascontiguousarray(stereo_wave, dtype=np.int16)
    sound = pygame.sndarray.make_sound(stereo_wave)
    sound.set_volume(0.2)
    sound.play()


def randomise(new_size):
    import random

    global array, size, bar_width
    size = max(2, min(int(new_size), width - 200))
    bar_width = max(1, (width - 200) // size)
    array = np.array([0 for i in range(size)])
    draw_array(array)
    pygame.display.flip()

    for i in range(size - 1):
        array[i] = (i / size) * (height - button_height - 2 * button_margin)
        draw_array(array, i, i + 1)
        play_tone(200 + (i * (880 - 200) / size))  # Play a tone scaled by the bar's position
        pygame.display.flip()
        pygame.time.wait(2)
        if handle_abort_events():
            return

    for i in range(size - 1):
        j = random.randint(i + 1, size - 1)
        array[i], array[j] = array[j], array[i]
        draw_array(array, i, j)
        play_tone(200 + (i * (880 - 200) / size))  # Play a tone scaled by the bar's position
        pygame.display.flip()
        pygame.time.wait(2)
        if handle_abort_events():
            return


def is_sorted(arr):
    for i in range(0, len(arr) - 1):
        if int(arr[i]) > int(arr[i + 1]):
            return False
    return True


############################################ BUBBLE SHORT ############################################

def bubble_sort(arr):
    global speed
    n = len(arr)
    for i in range(n):
        for j in range(0, n - i - 1):
            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        return;
            if arr[j] > arr[j + 1]:
                arr[j], arr[j + 1] = arr[j + 1], arr[j]
                draw_array(arr, j, j + 1)
                play_tone(200 + (j * (880 - 200) / size))  # Play a tone scaled by the bar's position
                pygame.display.flip()
                wait_for_speed()


############################################ SELECTION SHORT ###################################################################################

def selection_sort(arr):
    global speed
    n = len(arr)
    for i in range(n):
        # Find the minimum element in the remaining unsorted array
        min_idx = i
        for j in range(i + 1, n):
            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        return;
            if arr[min_idx] > arr[j]:
                min_idx = j

        # Swap the found minimum element with the first element
        arr[i], arr[min_idx] = arr[min_idx], arr[i]
        draw_array(arr, i, min_idx)  # Update the visual display
        play_tone(200 + (i * (880 - 200) / size))  # Play a tone scaled by the bar's position
        pygame.display.flip()
        wait_for_speed()


############################################ QUICK SHORT ###################################################################################

def quick_sort(arr, low, high):
    global speed
    if is_sorted(arr):
        return
    if low < high:
        pi = qs_partition(arr, low, high)
        if pi < 0:
            return
        quick_sort(arr, low, pi - 1)
        quick_sort(arr, pi + 1, high)


def qs_partition(arr, low, high):
    global speed
    pivot = arr[high]
    i = low - 1

    for j in range(low, high):
        for event in pygame.event.get():
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return -high
        if arr[j] <= pivot:
            i += 1
            arr[i], arr[j] = arr[j], arr[i]
            draw_array(arr, i, j)  # Update the visual display
            play_tone(200 + (i * (880 - 200) / size))  # Play a tone scaled by the bar's position
            pygame.display.flip()
            wait_for_speed()

    arr[i + 1], arr[high] = arr[high], arr[i + 1]
    play_tone(200 + (i * (880 - 200) / size))  # Play a tone scaled by the bar's position
    pygame.display.flip()
    wait_for_speed()
    return i + 1


############################################ PANCAKE SHORT ###################################################################################

def pancake_sort(arr):
    global array, speed

    def flip(arr, i):
        return arr[:i + 1][::-1] + arr[i + 1:]

    arr = list(arr)  # Convert the NumPy array to a list
    n = len(arr)
    for curr_size in range(n, 1, -1):
        max_index = arr.index(max(arr[:curr_size]))
        for event in pygame.event.get():
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    array = np.array(arr)
                    return
        if max_index != curr_size - 1:
            if max_index != 0:
                arr = flip(arr, max_index)
                draw_array(arr, max_index)
                play_tone(200 + (max_index * (880 - 200) / len(arr)))
                pygame.display.flip()
                wait_for_speed()
            arr = flip(arr, curr_size - 1)
            draw_array(arr, curr_size - 1)
            play_tone(200 + ((curr_size - 1) * (880 - 200) / len(arr)))
            pygame.display.flip()
            wait_for_speed()

    array = np.array(arr)  # Convert the list back to a NumPy array


############################################ HEAP SHORT ###################################################################################


def heapify(arr, n, i):
    global speed
    for event in pygame.event.get():
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_ESCAPE:
                array = np.array(arr)
                return False
    largest = i  # Initialize the largest element as the root
    left_child = 2 * i + 1
    right_child = 2 * i + 2

    # Check if the left child exists and is greater than the root
    if left_child < n and arr[left_child] > arr[largest]:
        largest = left_child

    # Check if the right child exists and is greater than the largest so far
    if right_child < n and arr[right_child] > arr[largest]:
        largest = right_child

    # If the largest element is not the root, swap them
    if largest != i:
        arr[i], arr[largest] = arr[largest], arr[i]
        draw_array(arr, i, largest)  # Update the visual display
        play_tone(200 + (i * (880 - 200) / len(arr)))  # Play a tone scaled by the bar's position
        pygame.display.flip()
        wait_for_speed()

        # Recursively heapify the affected sub-tree
        if not heapify(arr, n, largest):
            return False
    return True


def heap_sort(arr):
    global speed
    n = len(arr)

    # Build a max heap
    for i in range(n // 2 - 1, -1, -1):
        if not heapify(arr, n, i):
            return False

    # Extract elements one by one
    for i in range(n - 1, 0, -1):
        arr[i], arr[0] = arr[0], arr[i]  # Swap the root (maximum) element with the last element
        draw_array(arr, i, 0)  # Update the visual display
        play_tone(200 + (0 * (880 - 200) / len(arr)))  # Play a tone scaled by the bar's position
        pygame.display.flip()
        wait_for_speed()

        # Call heapify on the reduced heap
        if not heapify(arr, i, 0):
            return False


############################################ MERGE SHORT ###################################################################################

def merge(arr, l, m, r, draw_func):
    global speed
    n1 = m - l + 1
    n2 = r - m
    L = [0] * n1
    Li = [0] * n1
    R = [0] * n2
    Ri = [0] * n2

    for i in range(0, n1):
        L[i] = arr[l + i]
        Li[i] = l + i

    for j in range(0, n2):
        R[j] = arr[m + 1 + j]
        Ri[j] = m + 1 + j

    i = 0
    j = 0
    k = l

    while i < n1 and j < n2:
        if L[i] <= R[j]:
            arr[k] = L[i]
            draw_func(arr, k, Li[i])
            i += 1
        else:
            arr[k] = R[j]
            draw_func(arr, k, Ri[j])
            j += 1
        k += 1
        pygame.event.get()
    while i < n1:
        arr[k] = L[i]
        draw_func(arr, k, Li[i])
        i += 1
        k += 1
        pygame.event.get()
    while j < n2:
        arr[k] = R[j]
        draw_func(arr, k, Ri[j])
        j += 1
        k += 1
        pygame.event.get()
    # draw_func(arr,n1,n2)  # Now the draw function will be called after each merge


def merge_sort(arr, l, r, draw_func):
    global speed
    if l < r:
        m = l + (r - l) // 2

        merge_sort(arr, l, m, draw_func)
        merge_sort(arr, m + 1, r, draw_func)

        merge(arr, l, m, r, draw_func)
        for event in pygame.event.get():
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    raise Exception("Aborted by keypress");


def merge_sort_wrapper(arr):
    global speed

    def draw_and_play(arr, n1, n2):
        draw_array(arr, n1, n2)
        play_tone(200 + (n1 * (880 - 200) / size))
        pygame.display.flip()
        wait_for_speed()

    try:
        merge_sort(arr, 0, len(arr) - 1, draw_and_play)
    except:
        pass


################################################# COCKTAIL SHAKER SORT #############################################

def cocktail_shaker_sort(arr):
    global speed
    n = len(arr)
    swapped = True
    start = 0
    end = n - 1
    while swapped:
        swapped = False
        for i in range(start, end):
            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        return
            if arr[i] > arr[i + 1]:
                arr[i], arr[i + 1] = arr[i + 1], arr[i]
                draw_array(arr, i, i + 1)
                play_tone(200 + (i * (880 - 200) / size))
                pygame.display.flip()
                wait_for_speed()
                swapped = True

        if not swapped:
            break

        swapped = False
        end -= 1

        for i in range(end - 1, start - 1, -1):
            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        return
            if arr[i] > arr[i + 1]:
                arr[i], arr[i + 1] = arr[i + 1], arr[i]
                draw_array(arr, i, i + 1)
                play_tone(200 + (i * (880 - 200) / size))
                pygame.display.flip()
                wait_for_speed()
                swapped = True

        start += 1


################################################ RADIX SORT ############################################

def counting_sort_for_radix(arr, exp):
    n = len(arr)
    output = [0] * n
    count = [0] * 10

    for i in range(n):
        index = int(arr[i] // exp)  # Convert to integer
        count[index % 10] += 1

    for i in range(1, 10):
        count[i] += count[i - 1]

    i = n - 1
    while i >= 0:
        index = int(arr[i] // exp)  # Convert to integer
        output[count[index % 10] - 1] = arr[i]
        count[index % 10] -= 1
        i -= 1
        for event in pygame.event.get():  # Event handling inside the sorting loop
            if event.type == pygame.QUIT:
                pygame.quit()
                return

    for i in range(len(arr)):
        arr[i] = output[i]
        draw_array(arr, i, -1)
        play_tone(200 + (arr[i] * (880 - 200) / safe_max(arr)))  # Play a tone scaled by the bar's value
        pygame.display.flip()
        wait_for_speed()
        for event in pygame.event.get():  # Event handling inside the sorting loop
            if event.type == pygame.QUIT:
                pygame.quit()
                return


def radix_sort(arr):
    max_val = max(arr)
    exp = 1
    while max_val / exp > 0:
        counting_sort_for_radix(arr, exp)
        exp *= 10
        draw_array(arr)
        pygame.display.flip()
        wait_for_speed()
        if is_sorted(arr):
            return
        for event in pygame.event.get():
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return


############################################RADIX MERGE SORT#############################################


def counting_sort_for_radix_merge(arr, exp):
    n = len(arr)
    output = [0] * n
    count = [0] * 10

    for i in range(n):
        index = int(arr[i] // exp)  # Convert to integer
        count[index % 10] += 1

    for i in range(1, 10):
        count[i] += count[i - 1]

    i = n - 1
    while i >= 0:
        index = int(arr[i] // exp)  # Convert to integer
        output[count[index % 10] - 1] = arr[i]
        count[index % 10] -= 1
        i -= 1
        for event in pygame.event.get():  # Event handling inside the sorting loop
            if event.type == pygame.QUIT:
                pygame.quit()
                return False

    for i in range(len(arr)):
        arr[i] = output[i]
        draw_array(arr, i, -1)
        play_tone(200 + (arr[i] * (880 - 200) / safe_max(arr)))  # Play a tone scaled by the bar's value
        pygame.display.flip()
        wait_for_speed()
        for event in pygame.event.get():  # Event handling inside the sorting loop
            if event.type == pygame.QUIT:
                pygame.quit()
                return False

    return True


def merge_for_radix_merge(left, right):
    result = []
    i = j = 0
    while i < len(left) and j < len(right):
        if left[i] < right[j]:
            result.append(left[i])
            i += 1
        else:
            result.append(right[j])
            j += 1

        # Ensure proper visualization handling for different shapes
        current_merge_state = result + left[i:] + right[j:]
        if len(current_merge_state) > 0:
            draw_array(current_merge_state)  # Draw the current merge state
            pygame.display.flip()
            wait_for_speed()
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    return result + left[i:] + right[j:]

    result.extend(left[i:])
    result.extend(right[j:])
    return result


def merge_sort_for_radix_merge(arr):
    if len(arr) <= 1:
        return arr

    mid = len(arr) // 2
    left = merge_sort_for_radix_merge(arr[:mid])
    right = merge_sort_for_radix_merge(arr[mid:])

    return merge_for_radix_merge(left, right)


def radix_merge_sort(arr):
    max_val = max(arr)
    exp = 1
    while max_val / exp >= 1:
        if not counting_sort_for_radix_merge(arr, exp):
            return
        exp *= 10
        draw_array(arr)
        pygame.display.flip()
        wait_for_speed()
        if is_sorted(arr):
            return
        for event in pygame.event.get():
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return

    sorted_arr = merge_sort_for_radix_merge(arr)
    for i in range(len(arr)):
        arr[i] = sorted_arr[i]
        draw_array(arr, i, -1)
        play_tone(200 + (arr[i] * (880 - 200) / safe_max(arr)))  # Play a tone scaled by the bar's value
        pygame.display.flip()
        wait_for_speed()
        if is_sorted(arr):
            return
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                return


######################################## INSERTION SORT #############################################

def insertion_sort(arr):
    global speed
    for i in range(1, len(arr)):
        key = arr[i]
        j = i - 1
        while j >= 0 and key < arr[j]:
            arr[j + 1] = arr[j]
            j -= 1
            draw_array(arr, j + 1, j)  # Update the visual display
            play_tone(200 + (key * (880 - 200) / height))  # Play a tone scaled by the bar's value
            pygame.display.flip()
            wait_for_speed()
            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        return
        arr[j + 1] = key
        draw_array(arr, j + 1)  # Update the visual display for the final placement
        pygame.display.flip()
        wait_for_speed()


################################################### Tim Sort #######################################################

MIN_MERGE = 32


def calc_min_run(n):
    """Calculate the minimum run size."""
    r = 0
    while n >= MIN_MERGE:
        r |= n & 1
        n >>= 1
    return n + r


def tim_insertion_sort(arr, left, right):
    for i in range(left + 1, right + 1):
        j = i
        while j > left and arr[j] < arr[j - 1]:
            arr[j], arr[j - 1] = arr[j - 1], arr[j]
            j -= 1
            draw_array(arr, j, j - 1)  # Visualize the sorting
            play_tone(200 + (arr[j] * (880 - 200) / height))  # Play a tone scaled by the bar's value
            pygame.display.flip()
            wait_for_speed()
            for event in pygame.event.get():  # Event handling inside the sorting loop
                if event.type == pygame.QUIT:
                    pygame.quit()
                    return
                if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                    return


def tim_merge(arr, l, m, r):
    # Create temporary arrays
    L = arr[l:m + 1].copy()
    R = arr[m + 1:r + 1].copy()

    # Initial indexes for subarrays and main array
    i = j = 0
    k = l

    # Merging the temporary arrays back into arr[l..r]
    while i < len(L) and j < len(R):
        # Event handling inside the sorting loop
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                return
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                return

        # Choose the smaller of the two elements 'L[i]' and 'R[j]'
        if L[i] <= R[j]:
            arr[k] = L[i]
            i += 1
        else:
            arr[k] = R[j]
            j += 1

        # Visualization and sound
        draw_array(arr, k, k)  # Visualize the sorting
        play_tone(200 + (arr[k] * (880 - 200) / height))  # Play a tone scaled by the bar's value
        pygame.display.flip()
        wait_for_speed()
        k += 1

    # Copy any remaining elements of L[], if there are any
    while i < len(L):
        # Event handling inside the sorting loop
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                return
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                return

        arr[k] = L[i]
        i += 1
        k += 1

        # Visualization and sound
        draw_array(arr, k - 1, k - 1)  # Visualize the sorting
        play_tone(200 + (arr[k - 1] * (880 - 200) / height))  # Play a tone scaled by the bar's value
        pygame.display.flip()
        wait_for_speed()

    # Copy any remaining elements of R[], if there are any
    while j < len(R):
        # Event handling inside the sorting loop
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                return
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                return

        arr[k] = R[j]
        j += 1
        k += 1

        # Visualization and sound
        draw_array(arr, k - 1, k - 1)  # Visualize the sorting
        play_tone(200 + (arr[k - 1] * (880 - 200) / height))  # Play a tone scaled by the bar's value
        pygame.display.flip()
        wait_for_speed()


def tim_sort(arr):
    n = len(arr)
    min_run = calc_min_run(n)

    # Sort individual subarrays of size min_run
    for start in range(0, n, min_run):
        end = min(start + min_run - 1, n - 1)
        tim_insertion_sort(arr, start, end)

    # Start merging from size min_run (or 32). It will merge
    # to form size 64, then 128, 256 and so on ....
    size = min_run
    while size < n:
        # Pick starting point of left sub array. We
        # are going to merge arr[left..left+size-1]
        # and arr[left+size, left+2*size-1]
        # After every merge, we increase left by 2*size
        for left in range(0, n, 2 * size):
            # Find ending point of left sub array
            mid = min(n - 1, left + size - 1)
            right = min((left + 2 * size - 1), (n - 1))
            # Merge sub array arr[left.....mid] & arr[mid+1....right]
            if mid < right:
                tim_merge(arr, left, mid, right)
        size *= 2


############################################## Radix heap sort #################################

def counting_sort_for_radix_visualized(arr, exp, speed):
    n = len(arr)
    output = [0] * n
    count = [0] * 10

    for i in range(n):
        index = int((arr[i] // exp) % 10)
        count[index] += 1

    for i in range(1, 10):
        count[i] += count[i - 1]

    i = n - 1
    while i >= 0:
        index = int((arr[i] // exp) % 10)
        output[count[index] - 1] = arr[i]
        count[index] -= 1
        i -= 1

    for i in range(n):
        arr[i] = output[i]
        draw_array(arr, [i])
        play_tone(200 + (arr[i] * (880 - 200) / safe_max(arr)))
        pygame.display.flip()
        wait_for_speed()
        if handle_abort_events():
            return False
    return True

def radix_sort_visualized(arr, speed):
    max_val = max(arr)
    exp = 1
    while max_val // exp > 0:
        if not counting_sort_for_radix_visualized(arr, exp, speed):
            return False
        exp *= 10
    return True

def heapify_visualized(arr, n, i, speed):
    largest = i
    left = 2 * i + 1
    right = 2 * i + 2

    if left < n and arr[largest] < arr[left]:
        largest = left

    if right < n and arr[largest] < arr[right]:
        largest = right

    if largest != i:
        arr[i], arr[largest] = arr[largest], arr[i]
        draw_array(arr, [i, largest])
        play_tone(200 + (arr[i] * (880 - 200) / safe_max(arr)))
        pygame.display.flip()
        wait_for_speed()
        if handle_abort_events():
            return False
        return heapify_visualized(arr, n, largest, speed)
    return True

def heap_sort_visualized(arr, speed):
    n = len(arr)
    for i in range(n // 2 - 1, -1, -1):
        if not heapify_visualized(arr, n, i, speed):
            return False

    for i in range(n - 1, 0, -1):
        arr[i], arr[0] = arr[0], arr[i]
        draw_array(arr, [i, 0])
        play_tone(200 + (arr[i] * (880 - 200) / safe_max(arr)))
        pygame.display.flip()
        wait_for_speed()
        if handle_abort_events():
            return False
        if not heapify_visualized(arr, i, 0, speed):
            return False
    return True

def radix_heap_sort(arr):
    if not radix_sort_visualized(arr, speed):
        return
    heap_sort_visualized(arr, speed)

#########################################################################




# Buttons and Textboxes

# Define button rectangles
bubble_sort_button = pygame.Rect((button_margin, height - button_height - button_margin, button_width, button_height))
selection_sort_button = pygame.Rect(
    (2 * button_margin + button_width, height - button_height - button_margin, button_width, button_height))
quick_sort_button = pygame.Rect(
    (3 * button_margin + 2 * button_width, height - button_height - button_margin, button_width, button_height))

randomise_button = pygame.Rect(
    (width - button_margin - button_width, height - button_height - button_margin, button_width, button_height))
size_input = pygame.Rect(
    (1 * button_margin + 0 * button_width, height - button_height - button_margin, button_width, button_height))
speed_input = pygame.Rect(
    (2 * button_margin + 1 * button_width, height - button_height - button_margin, button_width, button_height))

bubble_sort_button = pygame.Rect(
    (width - button_margin - button_width, 1 * button_margin + 0 * button_height, button_width, button_height))
selection_sort_button = pygame.Rect(
    (width - button_margin - button_width, 2 * button_margin + 1 * button_height, button_width, button_height))
quick_sort_button = pygame.Rect(
    (width - button_margin - button_width, 3 * button_margin + 2 * button_height, button_width, button_height))
heap_sort_button = pygame.Rect(
    (width - button_margin - button_width, 4 * button_margin + 3 * button_height, button_width, button_height))
pancake_sort_button = pygame.Rect(
    (width - button_margin - button_width, 5 * button_margin + 4 * button_height, button_width, button_height))
merge_sort_button = pygame.Rect(
    (width - button_margin - button_width, 6 * button_margin + 5 * button_height, button_width, button_height))
cocktail_shaker_sort_button = pygame.Rect(
    (width - button_margin - button_width, 7 * button_margin + 6 * button_height, button_width, button_height))
radix_sort_button = pygame.Rect(
    (width - button_margin - button_width, 8 * button_margin + 7 * button_height, button_width, button_height))
insertion_sort_button = pygame.Rect(
    (width - button_margin - button_width, 9 * button_margin + 8 * button_height, button_width, button_height))
tim_sort_button = pygame.Rect(
    (width - button_margin - button_width, 10 * button_margin + 9 * button_height, button_width, button_height))
radix_merge_sort_button = pygame.Rect(
    (width - button_margin - button_width, 11 * button_margin + 10 * button_height, button_width, button_height))
radix_heap_sort_button = pygame.Rect(
    (width - button_margin - button_width, 12 * button_margin + 11 * button_height, button_width, button_height))

size_color = pygame.Color('lightskyblue3')
speed_color = pygame.Color('lightskyblue3')
size_text = f"{size}"
speed_text = f"{speed}"


def GetSizeText():
    return size_text


def GetSizeColour():
    return size_color


def draw_control(rect, text, text_colour, control_colour, font):
    pygame.draw.rect(screen, control_colour, rect)
    txt = font.render(text, True, text_colour)
    screen.blit(txt, rect.move(10, 10))


def draw_buttons():
    global size_color, speed_color
    font = pygame.font.SysFont(None, 18)

    draw_control(bubble_sort_button, 'Bubble Sort', GRAY, DARK_GRAY, font)
    draw_control(selection_sort_button, 'Selection Sort', GRAY, DARK_GRAY, font)
    draw_control(merge_sort_button, 'Merge Sort', GRAY, DARK_GRAY, font)
    draw_control(quick_sort_button, 'Quick Sort', GRAY, DARK_GRAY, font)
    draw_control(randomise_button, "Shuffle", GRAY, DARK_GRAY, font)
    draw_control(size_input, size_text, GRAY, size_color, font)
    draw_control(speed_input, speed_text, GRAY, speed_color, font)
    draw_control(pancake_sort_button, "Pancake Sort", GRAY, DARK_GRAY, font)
    draw_control(heap_sort_button, "Heap Sort", GRAY, DARK_GRAY, font)
    draw_control(cocktail_shaker_sort_button, 'Cocktail Shaker Sort', GRAY, DARK_GRAY, font)
    draw_control(radix_sort_button, 'Radix Sort', GRAY, DARK_GRAY, font)
    draw_control(insertion_sort_button, 'Insertion Sort', GRAY, DARK_GRAY, font)
    draw_control(tim_sort_button, 'Tim Sort', GRAY, DARK_GRAY, font)
    draw_control(radix_merge_sort_button, 'Radix Merge Sort', GRAY, DARK_GRAY, font)
    draw_control(radix_heap_sort_button, 'Radix Heap Sort', GRAY, DARK_GRAY, font)


def main():
    global sorting, size, size_color, size_text, speed_text, speed, speed_color
    color_active = pygame.Color('dodgerblue2')
    color_inactive = pygame.Color('lightskyblue3')
    active = ""
    running = True
    sorting = False
    screen.fill(WHITE)  # Clear the screen
    size_text = f"{size}"
    while running:
        for event in pygame.event.get():

            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if active == "Size":
                    if event.key == pygame.K_RETURN:
                        try:
                            size = max(2, min(int(size_text), width - 200))
                            size_text = str(size)
                            randomise(size)
                        except ValueError:
                            size_text = ''
                            active = ""
                            size_color = color_inactive
                    elif event.key == pygame.K_BACKSPACE:
                        size_text = size_text[:-1]
                    else:
                        size_text += event.unicode
                if active == "Speed":
                    if event.key == pygame.K_RETURN:
                        try:
                            speed = max(0, min(int(speed_text), 100))
                            speed_text = str(speed)
                            active = ""
                            speed_color = color_inactive
                        except ValueError:
                            speed_text = ''
                    elif event.key == pygame.K_BACKSPACE:
                        speed_text = speed_text[:-1]
                    else:
                        speed_text += event.unicode
                if event.key == pygame.K_ESCAPE:
                    sorting = False
                    running = False
            elif event.type == pygame.MOUSEBUTTONDOWN and not sorting:
                mouse_pos = event.pos
                if size_input.collidepoint(mouse_pos):
                    active = "Size" if active != "Size" else ""
                if speed_input.collidepoint(mouse_pos):
                    active = "Speed" if active != "Speed" else ""
                size_color = color_active if active == "Size" else color_inactive
                speed_color = color_active if active == "Speed" else color_inactive
                if bubble_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    bubble_sort(array)
                    sorting = False
                elif merge_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    merge_sort_wrapper(array)
                    sorting = False
                elif heap_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    heap_sort(array)
                    sorting = False
                elif pancake_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    pancake_sort(array)
                    sorting = False
                elif selection_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    selection_sort(array)
                    sorting = False
                elif quick_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    quick_sort(array, 0, len(array) - 1)
                    sorting = False
                elif randomise_button.collidepoint(mouse_pos):
                    sorting = False
                    randomise(size)
                    sorting = False
                elif cocktail_shaker_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    cocktail_shaker_sort(array)
                    sorting = False
                elif radix_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    radix_sort(array)
                    sorting = False
                elif insertion_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    insertion_sort(array)
                    sorting = False
                elif tim_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    tim_sort(array)
                    sorting = False
                elif radix_merge_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    radix_merge_sort(array)
                    sorting = False
                elif radix_heap_sort_button.collidepoint(mouse_pos):
                    sorting = True
                    radix_heap_sort(array)
                    sorting = False

        draw_array(array)
        draw_buttons()
        pygame.display.flip()  # Update the screen once per frame

    pygame.quit()


if __name__ == "__main__":
    main()
