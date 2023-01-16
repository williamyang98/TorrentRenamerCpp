import random
import argparse
import shutil
import os

# Generates a test directory with: 
# - parent series folder
# - renameable episode files
# - awaiting deleted files
# - whitelists files
# - whitelisted folders

# Test checked inputs
VALID_EPISODE_FORMATS = [
    "{title}s{season}e{episode}{name}{tags}{ext}",
    "{title}Season{season}Episode{episode}{name}{tags}{ext}",
    "{title}{season}x{episode}{name}{tags}{ext}",
    "{title}{season}{episode:02d}{name}{tags}{ext}"
]

WHITELIST_FOLDERS = [
    "Extras"
]

WHITELIST_FILENAMES = [
    "series.json",
    "episodes.json"
]

BLACKLIST_EXTENSIONS = [
    ".nfo", 
    ".exe"
]

WHITELIST_TAGS = [
    "DC", 
    "EXTENDED", 
    "ALT", 
    "ALTERNATE", 
    "UNCUT"
]

# Test unchecked inputs
NON_WHITELIST_FOLDER = [
    "UnknownFolder"
]

NON_WHITELIST_FILENAMES = [
    "some_random_text.txt",
    "a_random_picture.jpg"
]

NON_BLACKLIST_EXTENSIONS = [
    ".txt",
    "",
    ".jpg"
]

NON_WHITELIST_TAGS = [
    "RES360p",
    "CODECh634x"
]

class StringGenerator:
    def __init__(self, prefix):
        self.prefix = prefix
        self.counter = 0
    
    def generate(self):
        res = f"_{self.prefix}_{self.counter}_"
        self.counter += 1
        return res

def generate_directory(total_samples=1):
    title_generator = StringGenerator("title")
    name_generator = StringGenerator("name")
    rand_bool = lambda: random.random() > 0.5

    def generate_fixed_parameters():
        title = title_generator.generate() if rand_bool() else ""
        # NOTE: Reduce the number of seasons/episodes so we end up with some conflicts in generated result
        season = random.randint(0, 5)
        episode = random.randint(0, 10)
        name = name_generator.generate() if rand_bool() else ""
        return {
            "title": title,
            "season": season,
            "episode": episode,
            "name": name,
        }

    # Generate episodes file
    for formatter in VALID_EPISODE_FORMATS:
        valid_tags = "".join((f"[{tag}]" for tag in WHITELIST_TAGS))
        invalid_tags = "".join((f"[{tag}]" for tag in NON_WHITELIST_TAGS))

        # Generate perfectly valid filenames
        for _ in range(total_samples):
            for ext in NON_BLACKLIST_EXTENSIONS:
                yield formatter.format(**generate_fixed_parameters(), tags="", ext=ext)
                fixed_params = generate_fixed_parameters()
                yield formatter.format(**generate_fixed_parameters(), tags=valid_tags, ext=ext)
                fixed_params = generate_fixed_parameters()
                yield formatter.format(**generate_fixed_parameters(), tags=invalid_tags, ext=ext)
    
    # Whitelisted folder
    for folder in WHITELIST_FOLDERS:
        yield os.path.join(folder, "whitelisted_file.txt")
    
    # Blacklisted extensions
    for ext in BLACKLIST_EXTENSIONS:
        yield f"blacklisted_extension{ext}"

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", default="./tests", help="Directory to generate test data in")
    parser.add_argument("--reset", action="store_true", help="Delete the test directory")
    parser.add_argument("--total-series", default=3, type=int, help="Total series to generate")
    parser.add_argument("--total-samples", default=3, type=int, help="Multiply number of renameable filenames to generate")

    args = parser.parse_args()

    if args.reset:
        shutil.rmtree(args.dir, ignore_errors=True)
    

    for series in range(args.total_series):
        series_name = f"series_{series}"

        filepaths = generate_directory(total_samples=args.total_samples)
        for filepath in filepaths:
            relative_filepath = os.path.join(args.dir, series_name, filepath)
            os.makedirs(os.path.dirname(relative_filepath), exist_ok=True)
            with open(relative_filepath, "w+"):
                pass
            print(relative_filepath)