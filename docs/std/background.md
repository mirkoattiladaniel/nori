# std/background

```nori
import "std/background" as background
```

std/background: ask the system to run this program later, even if it is not running.

    import "std/background" as bg

    fn main() {
        if bg::woken() { check_for_updates()  return }     // started by the scheduler
        bg::every_hours(6)                                 // ask to be woken from now on
        ...the program...
    }

That `if` is the whole interface. The program is entered the same way either way, so whatever
it does when woken is written once, in Nori, and shares every function with the rest of it,
with no second copy of the logic in another language.

On Android this is not reachable from native code: a job hours from now is started by the
framework, and the framework starts classes. There is no C entry point for "wake up later".
So `roll` packages one small generic Java class that does nothing but load the program and
enter it. It is the same class for every program. Ask for it with `background = true` in the
manifest's [native.android] section.

If the program is already running, the wake-up is refused rather than entered: two runs in one
process share a heap and a scheduler, and entering twice at once corrupts both. A program that
is open does not need waking anyway, since it can check whenever it likes.
### `fn woken() -> Bool`

Was this run started by the scheduler rather than by a person?

Also true when the program was given `--nori-woken` on the command line, which is how a desktop
timer says so, and which lets this path be exercised without waiting six hours for a phone.

### `fn every_hours(hours: Int) -> Result<Int>`

Ask to be woken about every `hours` hours. The system decides when, batches wake-ups
with other apps' and defers them while the device is idle, so this is a request and never a
promise. Asking again replaces the previous request rather than adding one.

Ok(hours) when the request was accepted, Err(why) when there is nothing to accept it.

### `fn cancel() -> Bool`

Stop being woken. True when there was something to stop.


