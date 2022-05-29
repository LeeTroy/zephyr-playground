#include <zephyr.h>
#include <smf.h>
#include <shell/shell.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(state_machine, LOG_LEVEL_DBG);

enum demo_state {
	INIT,
	UNPROVISIONED,
	TMIN1,
	TZERO,
	VERIFY,
	RECOVERY,
	UPDATE,
	RUNTIME,
	LOCKDOWN
};
enum demo_event {
	INIT_DONE,
	VERIFY_UNPROVISIONED,
	VERIFY_PFM_FAILED,
	VERIFY_STG_FAILED,
	VERIFY_RCV_FAILED,
	VERIFY_ACT_FAILED,
	VERIFY_DONE,
	RECOVERY_FAILED,
	RECOVERY_DONE,
	RESET_DETECTED,
	UPDATE_REQUESTED,
	WDT_CHECKPOINT,
	UPDATE_DONE,
	UPDATE_FAILED,
	PROVISION_CMD,
};

K_FIFO_DEFINE(smf_fifo);

void do_init(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void enter_tmin1(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void exit_tmin1(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void enter_verify(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void do_verify(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void exit_verify(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void do_recovery(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void enter_tzero(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void exit_tzero(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void do_runtime(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void enter_update(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void do_update(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void exit_update(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void enter_lockdown(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

void do_lockdown(void *o)
{
	LOG_DBG("Start");
	LOG_DBG("End");
}

static const struct smf_state state_table[] = {
	[INIT] = SMF_CREATE_STATE(NULL, do_init, NULL, NULL),
	[TMIN1] = SMF_CREATE_STATE(enter_tmin1, NULL, exit_tmin1, NULL),
	[VERIFY] = SMF_CREATE_STATE(enter_verify, do_verify, exit_verify, &state_table[TMIN1]),
	[RECOVERY] = SMF_CREATE_STATE(NULL, do_recovery, NULL, &state_table[TMIN1]),
	[UPDATE] = SMF_CREATE_STATE(enter_update, do_update, exit_update, &state_table[TMIN1]),
	[TZERO] = SMF_CREATE_STATE(enter_tzero, NULL, exit_tzero, NULL),
	[UNPROVISIONED] = SMF_CREATE_STATE(NULL, NULL, NULL, &state_table[TZERO]),
	[RUNTIME] = SMF_CREATE_STATE(NULL, do_runtime, NULL, &state_table[TZERO]),
	// [SEAMLESS_UPDATE] = SMF_CREATE_STATE(NULL, do_seamless, NULL, NULL, &state_table[TZERO]),
	[LOCKDOWN] = SMF_CREATE_STATE(NULL, do_lockdown, NULL, NULL),
};


struct smf_context {
	/* Context data for smf */
	struct smf_ctx ctx;

	/* User Define State Data */
} s_obj;

struct event_context {
	/* Reserved for FIFO */
	void *fifo_reserved;

	/* User Defined Event */
	enum demo_event event;
	union {
		uint32_t bit32;
		uint8_t bit_array[4];
	} data;
};

void main()
{
	smf_set_initial(SMF_CTX(&s_obj), &state_table[INIT]);

	while (1) {
		struct event_context *fifo_in = (struct event_context *)k_fifo_get(&smf_fifo, K_FOREVER);
		if (fifo_in == NULL) {
			continue;
		}

		printk("EVENT IN [%p] EVT=%d\n", fifo_in, fifo_in->event);
		const struct smf_state *current_state = SMF_CTX(&s_obj)->current;
		const struct smf_state *next_state = NULL;
		bool run_state = false;

		if (current_state == &state_table[INIT]) {
			switch (fifo_in->event) {
			case INIT_DONE:
				next_state = &state_table[VERIFY];
				break;
			default:
				/* Discard anyother event */
				break;
			}
		} else if (current_state == &state_table[VERIFY]) {
			switch (fifo_in->event) {
			case VERIFY_UNPROVISIONED:
				next_state = &state_table[UNPROVISIONED];
				break;
			case VERIFY_PFM_FAILED:
			case VERIFY_STG_FAILED:
			case VERIFY_RCV_FAILED:
			case VERIFY_ACT_FAILED:
				next_state = &state_table[RECOVERY];
				break;
			case VERIFY_DONE:
				// Firmware is authenticated -> RUNTIME
				// Non provisioned -> UNPROVISIONED
				next_state = &state_table[RUNTIME];
				break;
			default:
				break;
			}
		} else if (current_state == &state_table[RECOVERY]) {
			switch (fifo_in->event) {
			case RECOVERY_DONE:
				next_state = &state_table[VERIFY];
				break;
			case RECOVERY_FAILED:
				next_state = &state_table[LOCKDOWN];
				break;
			default:
				break;			
			}
		} else if (current_state == &state_table[UPDATE]) {
			switch (fifo_in->event) {
			case UPDATE_DONE:
				next_state = &state_table[VERIFY];
				break;
			case UPDATE_FAILED:
				next_state = &state_table[RECOVERY];
				break;
			default:
				break;
			}
		} else if (current_state == &state_table[RUNTIME]) {
			switch (fifo_in->event) {
			case RESET_DETECTED:
				next_state = &state_table[VERIFY];
				break;
			case UPDATE_REQUESTED:
				// Check update intent, seamless or tmin1 update
				next_state = &state_table[UPDATE];
				break;
			case PROVISION_CMD:
				// Just run provision handling
				run_state = true;
			default:
				break;			
			}
		} else if (current_state == &state_table[UNPROVISIONED]) {
			switch (fifo_in->event) {
			case PROVISION_CMD:
				// Just run provision handling
				run_state = true;
				break;
			default:
				break;
			}
		}

		if (next_state) {
			smf_set_state(SMF_CTX(&s_obj), next_state);
		}
		
		if (run_state || next_state) {
			smf_run_state(SMF_CTX(&s_obj));
		}

		k_free(fifo_in);
	}
}


static int cmd_smf_event(const struct shell *shell,
		         size_t argc, char **argv, void *data)
{
	struct event_context *event = (struct event_context *)k_malloc(sizeof(struct event_context));
	printk("Send event %s to state machine\n", argv[0]);
	
	event->event = (enum demo_event)data;

	k_fifo_put(&smf_fifo, event);

	return 0;
}

static int cmd_smf_show(const struct shell *shell, size_t argc,
                        char **argv)
{
	printk("State List: \n");
	for (int i=0; i < ARRAY_SIZE(state_table); ++i) {
		printk("[%d] %p\n", i, &state_table[i]);
	}
	printk("Current state: %p\n", SMF_CTX(&s_obj)->current);
	return 0;
}


/* Creating subcommands (level 1 command) array for command "demo". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_smf,
        SHELL_CMD(show, NULL, "Show current state machine state", cmd_smf_show),
        SHELL_SUBCMD_SET_END
);
/* Creating root (level 0) command "demo" */
SHELL_CMD_REGISTER(smf, &sub_smf, "State Machine Commands", NULL);

SHELL_SUBCMD_DICT_SET_CREATE(sub_event, cmd_smf_event,
	(INIT_DONE, INIT_DONE),
	(VERIFY_UNPROVISIONED, VERIFY_UNPROVISIONED),
	(VERIFY_PFM_FAILED, VERIFY_PFM_FAILED),
	(VERIFY_STG_FAILED, VERIFY_STG_FAILED),
	(VERIFY_RCV_FAILED, VERIFY_RCV_FAILED),
	(VERIFY_ACT_FAILED, VERIFY_ACT_FAILED),
	(VERIFY_DONE, VERIFY_DONE),
	(RECOVERY_DONE, RECOVERY_DONE),
	(RECOVERY_FAILED, RECOVERY_FAILED),
	(RESET_DETECTED, RESET_DETECTED),
	(UPDATE_REQUESTED, UPDATE_REQUESTED),
	(UPDATE_DONE, UPDATE_DONE),
	(UPDATE_FAILED, UPDATE_FAILED),
	(PROVISION_CMD, PROVISION_CMD)
);

SHELL_CMD_REGISTER(event, &sub_event, "State Machine Events", NULL);

