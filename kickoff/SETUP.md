# Repo setup

    ninjago/
      CLAUDE.md          <- Claude Code reads this automatically every session
      KICKOFF.md         <- paste the part below the line as your first message
      build.py           <- source of truth for all game data
      dt_units.csv       <- generated: 209 units
      dt_factions.csv    <- generated: 17 factions
      dt_archetypes.csv  <- generated: 16 archetypes (the tuning table)
      dt_abilities.csv   <- generated: 44 abilities

Claude Code will move the CSVs into `Content/Data/` at M1.

Regenerate data at any time:

    python3 build.py --out Content/Data

Then re-run `Tools/import_datatables.py` inside the editor.
