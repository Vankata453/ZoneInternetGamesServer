# Internet Games DLL Injector

A DLL Injector, used to inject the [Internet Games Client DLL](/InternetGamesClientDLL) into any of the 3 Internet Games.

Before running the Injector, make sure a compiled `.dll` of the [Internet Games Client DLL](/InternetGamesClientDLL) project is available under the same directory, where the Injector executable resides.
It must be named "InternetGamesClientDLL.dll".

## Arguments

### Target Game

Specifying a target game is **required**.

* `-b` (`--backgammon`): Sets Internet Backgammon as the target game.
* `-c` (`--checkers`): Sets Internet Checkers as the target game.
* `-s` (`--spades`): Sets Internet Spades as the target game.

### Other

* `-r` (`--repeat`) [optional]: Skips a select number of previously started processes of the target game. Used when running multiple instances of the same game.
