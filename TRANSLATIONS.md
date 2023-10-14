# Translation

To translate Errands to your language you can use [Poedit]("https://flathub.org/ru/apps/net.poedit.Poedit")
1. Fork and clone repo.
2. Open Poedit.
- Open `po/errands.pot` file.
- Select your language and start translation. Remember to add your name and email in settings.
- Save it in `po` directory. Turn off compilation to `.mo` files on save in settings. We dont need those.
- Add your language in `po/LINGUAS` file separated by new line.
3. Test if you translation works by running `./build-aux/run.sh`.
5. Commit your changes and open a pull request.
6. Watch for updates in the future to provide additional translations.