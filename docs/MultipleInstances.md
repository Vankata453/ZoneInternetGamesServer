# Running multiple instances of an Internet Game (any version)

This document covers how the Windows 7/XP/ME Internet Games implement protection against creating multiple instances
of the same game, and how to bypass it for various purposes (for example, testing the Internet Games Server).

## How it works

On startup, the executable of any of the Internet Games creates a named mutex, using the [`CreateMutexW`](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createmutexw) function.

![Stack trace of the `CreateMutexW` function call](img/MultipleInstances_CreateMutexWTrace.png)

According to the function's documentation, its third parameter is `LPCWSTR lpName` - the name of the mutex object.

After we have hit the breakpoint on the function, we can check what `r8` points to.

![Value of `r8` at the time of the `CreateMutexW` function call](img/MultipleInstances_CreateMutexWR8.png)

Going to ```00000000`ff8f12b0``` in the memory viewer leads us to the following string:

![The value of the `LPCWSTR name` parameter, provided to the `CreateMutexW` function](img/MultipleInstances_CreateMutexWNameString.png)

This is the name of the mutex. If an instance of the same game already exists, the creation of the mutex fails for the following reason:

> If the mutex is a named mutex and the object existed before this function call, the return value is a handle to the existing object, and the GetLastError function returns ERROR_ALREADY_EXISTS.

If the mutex already exists, the game performs a few additional tasks, like focusing on the window of the other active instance, before closing.

Running multiple instances of different Internet Games is not an issue, since they all use a unique mutex name in their executable.

## Bypassing the protection

1. Install a Hex editor, like [HxD](https://mh-nexus.de/en/hxd/), which will be used here.

2. For Windows 7 games, go to "C:\Program Files\Microsoft Games\Multiplayer" and create a full copy of the game's folder. For other versions, simply copy the game executable.

![A copy of the "Checkers" folder](img/MultipleInstances_CheckersFolderCopy.png)

> [!NOTE]
>
> For Windows 7 games, a full folder copy needs to be done, because of a "Parental Controls" issue, when launching a renamed game executable.
>
> !["Parental Controls has blocked this game" error dialog](img/MultipleInstances_ParentalControlsIssue.png)
>
> In that case, the executable needs to use the same name, thus has to be in a different directory.
>
> The whole directory is copied, so the included DLLs are available from the new executable.

3. Start HxD as administrator and open the executable, located in the copy folder.

4. Search for the "_zm" Unicode string.

![HxD string search dialog](img/MultipleInstances_HxDStringSearch.png)
![HxD found mutex name string](img/MultipleInstances_HxDFoundString.png)

5. Modify any part of the found mutex name string, so it's different from the one in the main executable.

For example, let's change it from "mchkr_zm" to "mcckr_zm".

![HxD modified mutex name string](img/MultipleInstances_HxDModifiedString.png)

> [!NOTE]
>
> If you wish to run more than two instances of the same game, you must make sure this string is different in each executable.
> Different edits must be done.

6. Press Ctrl+S, or go to File -> Save to save the changes.

7. Launch both the original and the copied executable. You now have two running instances of the same game!

![Two instances of Internet Checkers running](img/MultipleInstances_Running.png)

You can create shortcuts to each executable for convenience.
