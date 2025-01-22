declare module "isolated-vm:capability/timers" {
	const schedule: (timeout: number) => void;
	export default schedule;
}
