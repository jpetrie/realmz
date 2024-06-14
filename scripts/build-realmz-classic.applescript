tell application "Finder"
	set script_path to parent of (path to me)
	set repo to parent of script_path
	set proj_file to ((repo as string) & ":proj:mac:Realmz Classic") as string
end tell

tell application "CodeWarrior IDE 4.0.4"
	activate

	open file proj_file

	set theTargets to {"Realmz Debug 68k", "Realmz Debug PPC", "Realmz Release 68k", "Realmz Release PPC", "Realmz Release FAT"}

	repeat with targ in theTargets
		Set Current Target targ
		Remove Binaries
		with timeout of 1200 seconds
			set theBuildInfo to Make Project with ExternalEditor
		end timeout
	end repeat
	
	Close Project
end tell
