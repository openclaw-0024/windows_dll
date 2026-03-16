using System.Runtime.InteropServices;
using System.Text;

internal static class Native
{
    private const string DllName = "libjoint_state_xrce_receiver.dll";

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void OnMessageCb(IntPtr jsonPayload, IntPtr userData);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int jsxrce_start(
        string agentIp,
        ushort agentPort,
        ushort domainId,
        string topicName,
        OnMessageCb callback,
        IntPtr userData);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void jsxrce_stop();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int jsxrce_is_running();
}

internal sealed class Program
{
    private static Native.OnMessageCb? _callbackHolder;

    private static void Main(string[] args)
    {
        string agentIp = args.Length > 0 ? args[0] : "192.168.60.80";
        ushort agentPort = ParseUShort(args, 1, 22018);
        string topicName = args.Length > 2 ? args[2] : "/xrce/kuka_joint_states_json";
        ushort domainId = ParseUShort(args, 3, 0);

        Console.OutputEncoding = Encoding.UTF8;
        Console.WriteLine("Starting C# receiver...");
        Console.WriteLine($"  agent_ip   : {agentIp}");
        Console.WriteLine($"  agent_port : {agentPort}");
        Console.WriteLine($"  topic_name : {topicName}");
        Console.WriteLine($"  domain_id  : {domainId}");

        _callbackHolder = OnMessage;

        int rc = Native.jsxrce_start(agentIp, agentPort, domainId, topicName, _callbackHolder, IntPtr.Zero);
        if (rc != 0)
        {
            Console.Error.WriteLine($"jsxrce_start failed: {rc}");
            Environment.ExitCode = 1;
            return;
        }

        Console.WriteLine("Receiving... press Enter to stop.");
        Console.ReadLine();

        Native.jsxrce_stop();
        Console.WriteLine("Stopped.");
    }

    private static ushort ParseUShort(string[] args, int index, ushort defaultValue)
    {
        if (args.Length <= index)
        {
            return defaultValue;
        }

        return ushort.TryParse(args[index], out ushort parsed) ? parsed : defaultValue;
    }

    private static void OnMessage(IntPtr jsonPayload, IntPtr _)
    {
        if (jsonPayload == IntPtr.Zero)
        {
            return;
        }

        string? json = Marshal.PtrToStringAnsi(jsonPayload);
        if (!string.IsNullOrEmpty(json))
        {
            Console.WriteLine($"RX: {json}");
        }
    }
}
