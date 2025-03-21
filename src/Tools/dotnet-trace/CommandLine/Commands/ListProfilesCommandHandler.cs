﻿// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Collections.Generic;
using System.CommandLine;
using System.CommandLine.Invocation;
using System.Diagnostics.Tracing;
using System.Threading.Tasks;
using Microsoft.Diagnostics.NETCore.Client;
using Microsoft.Diagnostics.Tracing.Parsers;

namespace Microsoft.Diagnostics.Tools.Trace
{
    internal sealed class ListProfilesCommandHandler
    {
        public static int GetProfiles()
        {
            try
            {
                foreach (Profile profile in DotNETRuntimeProfiles)
                {
                    Console.Out.WriteLine($"\t{profile.Name,-16} - {profile.Description}");
                }

                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[ERROR] {ex}");
                return 1;
            }
        }

        public static Command ListProfilesCommand()
        {
            Command listProfilesCommand = new(
                name: "list-profiles",
                description: "Lists pre-built tracing profiles with a description of what providers and filters are in each profile");

            listProfilesCommand.SetAction((parseResult, ct) => Task.FromResult(GetProfiles()));
            return listProfilesCommand;
        }

        internal static IEnumerable<Profile> DotNETRuntimeProfiles { get; } = new[] {
            new Profile(
                "cpu-sampling",
                new EventPipeProvider[] {
                    new("Microsoft-DotNETCore-SampleProfiler", EventLevel.Informational),
                    new("Microsoft-Windows-DotNETRuntime", EventLevel.Informational, (long)ClrTraceEventParser.Keywords.Default)
                },
                "Useful for tracking CPU usage and general .NET runtime information. This is the default option if no profile or providers are specified."),
            new Profile(
                "gc-verbose",
                new EventPipeProvider[] {
                    new(
                        name: "Microsoft-Windows-DotNETRuntime",
                        eventLevel: EventLevel.Verbose,
                        keywords: (long)ClrTraceEventParser.Keywords.GC |
                                  (long)ClrTraceEventParser.Keywords.GCHandle |
                                  (long)ClrTraceEventParser.Keywords.Exception
                    )
                },
                "Tracks GC collections and samples object allocations."),
            new Profile(
                "gc-collect",
                new EventPipeProvider[] {
                    new(
                        name: "Microsoft-Windows-DotNETRuntime",
                        eventLevel: EventLevel.Informational,
                        keywords: (long)ClrTraceEventParser.Keywords.GC
                    ),
                    new(
                        name: "Microsoft-Windows-DotNETRuntimePrivate",
                        eventLevel: EventLevel.Informational,
                        keywords: (long)ClrTraceEventParser.Keywords.GC
                    )
                },
                "Tracks GC collections only at very low overhead.") { RundownKeyword = (long)ClrTraceEventParser.Keywords.GC, RetryStrategy = RetryStrategy.DropKeywordDropRundown },
            new Profile(
                "database",
                new EventPipeProvider[] {
                    new(
                        name: "System.Threading.Tasks.TplEventSource",
                        eventLevel: EventLevel.Informational,
                        keywords: (long)TplEtwProviderTraceEventParser.Keywords.TasksFlowActivityIds
                    ),
                    new(
                        name: "Microsoft-Diagnostics-DiagnosticSource",
                        eventLevel: EventLevel.Verbose,
                        keywords:   (long)DiagnosticSourceKeywords.Messages |
                                    (long)DiagnosticSourceKeywords.Events,
                        arguments: new Dictionary<string, string> {
                            {
                                "FilterAndPayloadSpecs",
                                    "SqlClientDiagnosticListener/System.Data.SqlClient.WriteCommandBefore@Activity1Start:-Command;Command.CommandText;ConnectionId;Operation;Command.Connection.ServerVersion;Command.CommandTimeout;Command.CommandType;Command.Connection.ConnectionString;Command.Connection.Database;Command.Connection.DataSource;Command.Connection.PacketSize\r\n" +
                                    "SqlClientDiagnosticListener/System.Data.SqlClient.WriteCommandAfter@Activity1Stop:\r\n" +
                                    "Microsoft.EntityFrameworkCore/Microsoft.EntityFrameworkCore.Database.Command.CommandExecuting@Activity2Start:-Command.CommandText;Command;ConnectionId;IsAsync;Command.Connection.ClientConnectionId;Command.Connection.ServerVersion;Command.CommandTimeout;Command.CommandType;Command.Connection.ConnectionString;Command.Connection.Database;Command.Connection.DataSource;Command.Connection.PacketSize\r\n" +
                                    "Microsoft.EntityFrameworkCore/Microsoft.EntityFrameworkCore.Database.Command.CommandExecuted@Activity2Stop:"
                            }
                        }
                    )
                },
                "Captures ADO.NET and Entity Framework database commands")
        };

        /// <summary>
        /// Keywords for DiagnosticSourceEventSource provider
        /// </summary>
        /// <remarks>See https://github.com/dotnet/corefx/blob/master/src/System.Diagnostics.DiagnosticSource/src/System/Diagnostics/DiagnosticSourceEventSource.cs</remarks>
        private enum DiagnosticSourceKeywords : long
        {
            Messages = 0x1,
            Events = 0x2,
            IgnoreShortCutKeywords = 0x0800,
            AspNetCoreHosting = 0x1000,
            EntityFrameworkCoreCommands = 0x2000
        }
    }
}
