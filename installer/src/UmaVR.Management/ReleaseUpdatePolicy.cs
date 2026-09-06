using System.Security.Cryptography;

namespace UmaVR.Management;

public static class ReleaseUpdatePolicy
{
    public static bool IsNewer(string currentVersion, string releaseTag)
    {
        Version current = ParseVersion(currentVersion);
        Version release = ParseVersion(releaseTag);
        return release > current;
    }

    public static Version ParseVersion(string value)
    {
        string normalized = value.Trim();
        if (normalized.StartsWith('v') || normalized.StartsWith('V'))
        {
            normalized = normalized[1..];
        }

        int suffix = normalized.IndexOfAny(['-', '+']);
        if (suffix >= 0)
        {
            normalized = normalized[..suffix];
        }

        if (!Version.TryParse(normalized, out Version? parsed) ||
            parsed.Major < 0 || parsed.Minor < 0)
        {
            throw new InvalidDataException($"Invalid release version: {value}");
        }
        return parsed;
    }

    public static string ParseSha256(string value, string assetName)
    {
        foreach (string rawLine in value.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries))
        {
            string line = rawLine.Trim();
            if (line.StartsWith("sha256:", StringComparison.OrdinalIgnoreCase))
            {
                return RequireSha256(line[7..].Trim());
            }

            string[] parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 1)
            {
                return RequireSha256(parts[0]);
            }
            if (parts.Length >= 2 &&
                string.Equals(parts[^1].TrimStart('*'), assetName, StringComparison.Ordinal))
            {
                return RequireSha256(parts[0]);
            }
        }
        throw new InvalidDataException("The release checksum does not name the expected asset.");
    }

    public static string FileSha256(string path)
    {
        using FileStream stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream));
    }

    private static string RequireSha256(string value)
    {
        string normalized = value.Trim().ToUpperInvariant();
        if (normalized.Length != 64 || normalized.Any(character =>
            !Uri.IsHexDigit(character)))
        {
            throw new InvalidDataException("The release SHA-256 value is invalid.");
        }
        return normalized;
    }
}
