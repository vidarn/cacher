(
	rollout cacherRollout "Cacher"(
		group "Range" (
			spinner startFrame "Start" range:[0,99999,0] type:#integer across:2
			spinner endFrame "End" range:[0,99999,100] type:#integer
		)
		group "Ouput" (
			checkbox useObjName "Use Object Name" checked:true
			checkbox createSubFolder "Create subfolder" checked:true
			label nameLabel "Filename:" enabled:false
			edittext nameText "" enabled:false text:"Cache"
			label pathLabel "Output folder:"
			edittext pathText "" text:"C:\\Temp"
			button browseButton "Browse..."
		)
		button doItButton "Cache selected object"
		on useObjName changed val do (
			nameLabel.enabled = not val
			nameText.enabled = not val
		)
		on browseButton pressed do (
			local txt = getSavePath caption:"Output folder" initialDir:pathText.text
			if txt != undefined then (
				pathText.text = txt
			)
		)
		on doItButton pressed do (
			local sel = selection as array
			if sel.count == 1 then (
				local obj = sel[1]
				local filename = nameText.text
				if useObjName.checked then (
					filename = obj.name
				)
				local filepath = pathText.text
				if createSubFolder.checked then (
					filepath = pathConfig.appendPath filepath filename
					makedir filepath 
				)
				progressStart ("Caching " + filename)
				inv_frame_range = (100.0/(endFrame.value - startFrame.value))
				skip = false
				for i = startFrame.value to endFrame.value do (
					if not skip then (
						CacherCommands.CacheMesh obj (pathConfig.appendPath filepath filename) i i
						if (progressUpdate (i*inv_frame_range)) == false then (
							skip = true
						)
					)
				)
				progressEnd()
			)
			else (
				messageBox "Select one object"
			)
		)
	)
	createdialog cacherRollout width:250
)