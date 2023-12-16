def choose_option(title, options):
    """Choose one of the given options.

    options is a dictionary that maps internal options (keys) to readable strings (values).

    """

    print(title)
    print()
    keys = list(options.keys())
    strkeys = [str(k) for k in keys]
    for k in keys:
        print(f"{str(k):>3} = {options[k]}")

    print("  q = Abort")

    while True:
        inp = input("Type the entry shorthand: ")

        if inp == 'q':
            return None

        if inp not in strkeys:
            print("No valid entry selected. Try again.")
            continue

        return inp
