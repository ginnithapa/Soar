/**
 * 
 */
package edu.umich.soar.sproom.soar.commands;

import edu.umich.soar.sproom.Adaptable;
import edu.umich.soar.sproom.command.Comm;

import sml.Identifier;

/**
 * @author voigtjr
 *
 * Removes all messages from message list.
 */
public class ClearMessagesCommand extends OutputLinkCommand {
	static final String NAME = "clear-messages";

	private final Identifier wme;
	private boolean complete = false;

	public ClearMessagesCommand(Identifier wme) {
		super(Integer.valueOf(wme.GetTimeTag()));
		this.wme = wme;
	}

	@Override
	public OutputLinkCommand accept() {
		CommandStatus.accepted.addStatus(wme);
		return this;
	}
	
	@Override
	public String getName() {
		return NAME;
	}

	@Override
	public void update(Adaptable app) {
		if (!complete) {
			Comm comm = (Comm)app.getAdapter(Comm.class);
			comm.clearMessages();
			CommandStatus.complete.addStatus(wme);
			complete = true;
		}
	}
}