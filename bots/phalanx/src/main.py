"""Dispatch draft and gameplay; a failed turn is logged and still yields."""
import cambc
import draft
from bot import Bot


def main():
    bot = Bot()
    while True:
        try:
            if cambc.is_draft():
                draft.play()
            else:
                bot.play_turn()
        except Exception as error:
            # Keep this cheap: formatting a traceback reads SDK sources and can burn several turns of fuel.
            tb = error.__traceback__
            while tb.tb_next is not None:
                tb = tb.tb_next
            print(f"bot error: {type(error).__name__}: {error} (line {tb.tb_lineno} in {tb.tb_frame.f_code.co_name})",
                  flush=True)
        cambc.end_turn()


if __name__ == "__main__":
    main()
